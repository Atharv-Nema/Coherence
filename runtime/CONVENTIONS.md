# Runtime invariants

This document describes various aspects of the runtime, with the goal of making reasoning about the concurrency logic easier.


## 0. General background

The runtime consists of the following components.

### 0.1 Scheduler threads

* The runtime runs **32 OS threads** dedicated to scheduling and executing actor work.
* Each scheduler thread repeatedly pulls runnable actors from the global schedule queue and executes their pending behaviour calls.

### 0.2 Schedule queue

* The schedule queue holds identifiers to `ActorInstanceState` for actor instances that are eligible to run.
* Only actor instances in the `RUNNABLE` configuration may appear in this queue.

### 0.3 ActorInstanceState

`ActorInstanceState` stores the runtime-managed state of a single actor instance. Relevant fields:

* **mailbox**: a thread safe queue of pending behaviour calls
* **state**: one of
  * `EMPTY` — mailbox empty, no work to run
  * `RUNNABLE` — has work and is enqueued in the schedule queue
  * `RUNNING` — currently executing on a scheduler thread
  * `WAITING` — suspended (e.g., blocked on a lock) and parked in some wait queue (e.g. the mutex queue)
* **continuation**: optional resume point used to restart a suspended behaviour
* **actor_instance_id**: stable identifier of the actor instance

### 0.4 User-level mutex

* The runtime maintains a **mutex map** that allows the runtime to access  **user-level mutexes** (locks) used by actors.
* Each mutex includes:
  * its current ownership state (or equivalent atomic state)
  * a wait queue of **waiting** actor instances

### 0.5 Thread bed (idle waiting + termination)

* The runtime provides a **thread_bed**, which is an OS-level **binary semaphore**, used for:

  * allowing idle scheduler threads to sleep when there is no runnable work
  * coordinating **termination logic** (ensuring threads can stop cleanly once global quiescence is reached).
* Any transition that makes work available must signal the thread bed so that one sleeping scheduler thread wakes up, and work continues.



## 1. Stable `ActorInstanceState` configurations

These configurations specify the typical *stable* configuration. They are not always held, but some principles are defined in terms of them, and also typically, `ActorInstanceState` should be in one of these configurations, transitioning safely.

### 1.1 WAITING configuration

* **State:** `WAITING`
* **Mailbox:** non-empty, with the next element corresponding to the message associated with the suspended behaviour; this message is ignored by the continuation but is retained to preserve uniform handling and simplify the scheduling logic
* **Continuation:** the *waiting* continuation (i.e., where execution resumes after the lock becomes available)
* **Queue membership:** **not** in scheduler queue; **is** in exactly one lock’s wait queue

### 1.2 EMPTY configuration

* **State:** `EMPTY`
* **Mailbox:** empty
* **Continuation:** `nullptr`
* **Queue membership:** in no queue

### 1.3 RUNNABLE configuration

* **State:** `RUNNABLE`
* **Mailbox:** non-empty
* **Continuation:** may be `nullptr` or non-null (depending on whether there is suspended work to resume)
* **Queue membership:** in scheduler queue; **not** in any lock wait queue

---

### 1.4 RUNNING configuration

* **State:** `RUNNING`
* **Mailbox:** may or may not be empty
* **Continuation:** no invariants on this (no code uses it currently)
* **Queue membership:** in no queue

---

## 2. Global principles

### Principle A — one-shot transitions use CAS

In many cases, there are situations where I want to make a transition, and perform an action corresponding to that transition
exactly once. In these cases, do the transition using `compare_exchange_strong` and only perform the action if the transition succeeds.

### Principle B — queue-visible instances must be fully valid

Any actor instance stored in **any** queue must satisfy one of the **stable configurations** above.
No partially-initialized or “mid-transition” instances may be enqueued.

## 3. Message send logic

Goal: ensure that sending a message either:

* just appends to mailbox (if the actor is already in, or eventually will be in some queue (`WAITING`, `RUNNABLE`, `RUNNING`))
* wakes the actor by transitioning `EMPTY → RUNNABLE` and enqueuing it exactly once.

### 3.1 Algorithm

1. **Append message** to `mailbox` (always).
2. Read `state`:

   * If `state ∈ {WAITING, RUNNABLE, RUNNING}`: **do nothing else**.
   * If `state == EMPTY`:

     * Attempt `CAS(EMPTY → RUNNABLE)`.
     * If CAS succeeds: **enqueue** into scheduler queue.
     * If CAS fails: someone else changed the state; **do not enqueue**.

## 4. Lock acquisition logic

### 4.1 Assumptions:

1. **Double-locks never occur** (the same actor does not attempt to acquire a lock it already holds).
2. `ActorInstanceState` is currently in a valid `RUNNING` configuration


### 4.2 Post-conditions:

Atomically either:
* acquire the lock and continue, or
* set the actor in a valid `WAITING` configuration and enqueue it into the lock’s wait queue.

### 4.3 Algorithm:

1. Attempt to acquire the lock using `compare_exchange_strong` on the lock’s ownership state.
2. If lock CAS succeeds:

   * Actor continues execution immediately.
   * Return success.
3. If lock CAS fails:

   * Prepare actor’s **WAITING configuration**:
     * set `actor.state = WAITING`
     * set `actor.continuation = waiting_continuation` (resume point after lock)
     * push the behaviour message to the head of the actor mailbox
   * Enqueue actor into `lock.wait_queue`

## 5. Unlock logic

Goal: wake exactly one waiter (if any) and make it runnable.

### 5.1 Algorithm:

1. Perform lock-specific logic:
   * pop next actor from `lock.wait_queue` (if any)
   * update lock ownership data as required

2. If a waiting actor was popped:

   * Transition actor to a valid **RUNNABLE configuration**:

     * set `actor.state = RUNNABLE`
     * Enqueue actor into scheduler queue

---

## 6. Scheduler execution logic

This section describes how a scheduler thread runs an actor instance, focusing only on the key cases produced by the suspend tag.


### 6.1 High-level loop idea

For each scheduling step, a scheduler thread does:

1. Pop an `actor_instance_id` from the **schedule queue**.
2. Look up `ActorInstanceState` and transition from `RUNNABLE -> RUNNING`:
3. Pop the next mailbox message `msg` (the behaviour call to execute).
4. Ensure the actor has an execution context:
   * if `next_continuation == nullptr`, allocate a stack and create an initial continuation.
5. Enter an **inner loop** that repeatedly resumes execution by jumping into `next_continuation` with `msg`.
6. Each jump returns a `SuspendTag`; the scheduler handles the tag, and either **continues the inner loop** (e.g., lock acquired) or **exits the inner loop** (e.g., return or lock-blocked).


### 6.2 Case: behaviour returns (`SuspendTagKind::RETURN`)

**What the scheduler does:**

* Destroy the continuation:
  * `next_continuation = nullptr`
  * free `running_be_sp`
* Transition the actor to an idle configuration:
  * set `state = EMPTY`
* If there is still work (mailbox non-empty), reschedule exactly once:
  * attempt `CAS(EMPTY → RUNNABLE)`
  * if it succeeds: enqueue into schedule queue and wake a thread
* Break from the inner loop


### 6.3 Case: tries to lock and succeeds (`SuspendTagKind::LOCK` + acquire succeeds)

**What the scheduler does:**

* Save the returned continuation (the runtime must continue from the lock point):

  * `next_continuation = t.fctx`
* Acquire succeeds, so the actor can keep running immediately. The loop is not exited

### 6.4 Case: tries to lock and fails (`SuspendTagKind::LOCK` + acquire fails)

**What the scheduler does:**

* Save the returned continuation so the actor can resume later:

  * `next_continuation = t.fctx`
* Put the actor into a valid waiting configuration:

  * `state = WAITING`
  * push the current `msg` back onto the mailbox front (so the suspended behaviour has a “current message” when it resumes)
* Break from the inner loop

## 7. Thread sleeping mechanism

TO ADD LATER
