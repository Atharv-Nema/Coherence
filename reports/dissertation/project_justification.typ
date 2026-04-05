#set text(size: 12pt, font: "Linux Libertine")
#set heading(numbering: "1.1")
#set page(paper: "a4", margin: 2cm)
#set par(justify: true)

= Purpose
This document describes the core motivations behind _Coherence_. The main premise of the project is that many of the current actor models do not have a sufficiently expressive way to manage shared, mutable state across multiple actors without sacrificing either performance, safety, or simplicity. This document explores prior works in languages for concurrent programming and details how _Coherence_ integrates these to provide a safer and more expressive model.

= Pure-actor model
In the pure actor model, communication is restricted by message passing. Data is shared by copying rather than by reference. While this ensures state isolation, it is fundamentally inefficient for large-scale data processing:
- *Overhead:* Copying large data structures between actors is innefficient
- *Latency:* The cost of serialization and deserialization often outweighs the benefits of concurrent execution.

= The Pony approach
Pony safely reintroduces _shared memory_ using reference capabilities. This ensures data-race freedom at compile time. However, representing patterns where multiple actors frequently modify a shared data structure remains awkward. Two general patterns predominate:

== The manager pattern
This pattern encapsulates data within a "Manager" actor. All other actors interact with the data via asynchronous requests.

```pony
actor IntManager
  var _data: U64 = 0

  // The 'get' behavior takes in a callback
  be get(cb: {(U64): None} val) =>
    cb(_data)

  be update(value: U64) =>
    _data = value
```
In the code above, threads asynchronously send update or query requests as messages. For querying, the `IntManager` is given a safe, sendable callback to which it sends the current value of `_data`.

Let us consider the problem of swapping two shared integers.

```pony
actor Swapper
  be swap(m1: IntManager, m2: IntManager) =>
    // Step 1: Request value from the first manager
    m1.get({(v1: U64) val => 
      // Step 2: Request value from the second manager
      m2.get({(v2: U64) val => 
        // Step 3: Perform the cross-updates
        m1.update(v2)
        m2.update(v1)
      } val)
    } val)
```

The awkwardness of the manager approach is clearly demonstrated.

- Even with lambdas, the code is cluttered with nested closures.
- This version fails to provide atomicity. Between Step 2 and Step 3, a third actor could update `m1` and `m2`. The swapper would then overwrite that new value with stale data.

== Hot-Potato Model
To solve the atomicity problem, developers use the *hot-potato* model. Here, the manager transfers exclusive ownership of the object to the requester using the `iso` reference capability. The ownership is returned when the requester is done with the task.

```pony
// A simple shared container to hold our value
class IntContainer
  var value: U64 = 0
  new create(v: U64) => value = v

actor IntManager
  var _data: (IntContainer iso | None)

  new create(v: IntContainer iso) => _data = consume v

  // "Taking" the potato: the manager gives up the reference
  be take(cb: {(IntContainer iso): None} val) =>
    // In Pony, destructive writes return the overwritten value
    var potato: (IntContainer iso | None) = (_data = None)
    if potato is (let p: IntContainer iso) then
      cb(consume p)

  // Returning the potato
  be give(p: IntContainer iso) =>
    _data = consume p  
```

This code can be made more robust by ensuring that the `take` operation never fails. If the `IntManager` is currently empty (because another actor is holding the "potato"), it stores the incoming callback in a FIFO queue. When the potato is eventually returned via `give`, the manager automatically dispatches it to the next callback in the queue.

Assumming this more robust `IntManager`, the code to swap the two shared integers can be written as:

```pony
actor Swapper
  be swap(m1: IntManager, m2: IntManager) =>
    // Step 1: Queue a request for the first potato
    m1.take({(p1: IntContainer iso) val => 
      
      // Step 2: Once p1 is received, queue a request for the second
      m2.take({(p2: IntContainer iso) val =>
        
        // Step 3: We now hold both exclusively. 
        // No other actor can intervene because we possess the memory.
        let temp = p1.value
        p1.value = p2.value
        p2.value = temp

        // Step 4: Return the potatoes to wake up any pending waiters
        m1.give(consume p1)
        m2.give(consume p2)
      } val)
    } val)
```

While this queued hot-potato achieves atomic swaps, this approach is also flawed.
- The system relies entirely on the requester to return the reference via the `give` behavior. If a programmer forgets to return the "potato", the resource is effectively lost, and all other actors in the queue are blocked indefinitely.
- A more fundamental flaw is the indirect introduction of deadlocks. Because ownership is acquired sequentially across multiple actors, the system is susceptible to the "circular wait" condition.

Consider two actors attempting to access the same two managers simultaneously:

- Actor A calls `m1.take()` and successfully receives `p1`. It then calls `m2.take()` and enters `m2`'s wait queue.
- Actor B calls `m2.take()` and successfully receives `p2`. It then calls `m1.take()` and enters `m1`'s wait queue.

Now, Actor A is waiting for Actor B to call `m2.give()`, and Actor B is waiting for Actor A to call `m1.give()`. Neither can proceed, and the system is stuck. Pony programs terminate when there are no more messages to run. So in this case, the program simply terminates prematurely.

#block(
fill: gray.lighten(90%),
inset: 15pt,
radius: 5pt,
stroke: (left: 3pt + gray)
)[
 *Motivation*

The primary goal of _Coherence_ is to bridge the gap between the safety of the Actor model and the expressive power of imperative programming.

A developer should not have to manage nested callbacks or manual "potato" queues just to perform a swap. Instead, they should be able to express the operation as a single, atomic unit of work where the compiler handles the underlying synchronization. The language we are striving towards should allow programmers to do something like:

```
atomic {
  var temp: int = *a;
  *a = *b;
  *b = temp;
}
```
]

= _Locked_ reference capability

To overcome some of the limitations of the Pony model, we introduce the `locked` reference capability. This draws inspiration from a capability sharing the same name used in Kappa.
The idea of `locked` in Kappa is that an object with this capability comes with an implicit lock. Whenever a method of the object is called, the lock associated with the object is taken, which is released after the method execution completes. This object can be shared between actors, and hence allows for easy sharing of mutable state.

We can rewrite the swap logic using the `locked` capability.
```
class SharedInt
  var _data: U64 = 0
  fun read(): U64 => 
    return _data;
  fun write(v: U64) => 
    _data = v

actor Swapper
  be swap(m1: SharedInt locked, m2: SharedInt locked) =>
    let v1 = m1.read()
    let v2 = m2.read()
    m1.write(v2)
    m2.write(v1)
```

The code is clean and imperative. However, some issues still remain:
- Atomicity is not achieved. As locks are released after every operation, it is still possible for another actor to modify the shared integer values between the read and the write.
- In some contrived examples, deadlocks can still occur. This occurs when object fields contain locked references.

  ```pony
  class Node
    // A field pointing to another locked object
    var _next: (Node locked | None) = None
    new create(next: (Node locked | None)) =>
      _next = next
    // A dummy member function of Node
    fun access() => ...
    var access_next() =>
      // This will take the implicit lock of _next
      match _next
      | let n: Node locked => n.access()
      end
  ```
  Consider two `Node` instances, A and B, where `A._next = B` and `B._next = A`. If two separate actors call `access_next()` on A and B simultaneously, the following execution sequence can occur:

  - Actor 1 acquires the lock for A and enters `access_next()`.
  - Actor 2 acquires the lock for B and enters `access_next()`.
  - Actor 1 attempts to lock  (via `_next`) to call `access()`.
  - Actor 2 attempts to lock  (via `_next`) to call `access()`.

  This sequence leads to a deadlock.

= Atomic-sections and lock-set inference

To address the vulnerabilities of manual locking, Gudka introduced a system of automatic lock-inference for Java. This model utilizes atomic sections to allow programmers to define regions of code that must execute without interruption.

Instead of acquiring locks on-the-fly, the compiler statically analyses the atomic section to identify every lock that could potentially be required. The system attempts to acquire the entire lock-set before the start of the atomic-section. If any lock in the set is unavailable, the runtime immediately releases all currently held locks and retries the entire acquisition process.

This approach effectively rules out deadlocks and eliminates the atomicity problem by ensuring no other thread can intervene during the atomic section's execution.

= Integration in _Coherence_

_Coherence_ borrows the concepts mentioned above from Pony, Kappa and Gudka's work to create a language for safe and expressive concurrent programming. 

_Coherence_ retains Pony's standard reference capabilities (`iso`, `val`, `ref`, `tag`). It introduces the `locked<LOCK_NAME>` capability to facilitate shared-memory concurrency. Any pointer with this capability is protected by the lock `LOCK_NAME`. Unlike Kappa, programmers are expected to label the locks that protect a particular pointer (the locks are explicit). A pointer with a `locked` capability can only be dereferenced within an atomic section. Drawing inspiration from Gudka's work, a lock-set inference algorithm is run to determine all possible locks that can be acquired within the atomic-section. These locks are acquired upfront in a canonical order and held until the end of the section, ensuring both atomicity and deadlock-freedom.

```pony
actor Swapper
  be swap(ptr1: int locked<A>, ptr2: int locked<B>) =>
    // The compiler infers that both lock A and lock B are required.
    // They are acquired in a fixed order (e.g., A then B) before 
    // the code below executes.
    atomic {
      let temp = *ptr1
      *ptr1 = *ptr2
      *ptr2 = temp
    }
    // Both locks are automatically released here.
```

The code above in _Coherence_ provides a clean solution to the shared-memory swapping problem.

