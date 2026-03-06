#set text(size: 12pt, font: "Linux Libertine")
#set heading(numbering: "1.1")
#set page(paper: "a4", margin: 2cm)
#set par(justify: true)
#show "—": it => sym.wj + it

= Preparation

This chapter goes through the foundational concepts required to motivate and develop _Coherence_.

== Actor Model in Pony
The Pony programming language@pony_language is built following the actor model. However, Pony introduces specific variations to the pure actor model described in the introduction to better support high-performance programming. This section explores Pony's actor model in detail.

=== Basic syntax and semantics
In Pony, an _actor_ is a template to create actor-instances (just like a _class_ is a template to create _objects_). In the rest of the document, I would sometimes refer to actors as the template and sometimes as the specific instances when the meaning is not ambiguous. In ambiguous situations, I would use the terms _actor-template_ and _actor-instance_.

A Pony actor consists of three primary components:

*State:* This is the private data stored within the actor-instance. This data cannot be accessed directly by any other actor-instance.

*Constructors (`new`):* These are special functions used to intialise the state of an actor-instance upon creation.

*Behaviours (`be`):* These are the asynchronous interface through which actors communicate. Unlike standard synchronous functions, a behaviour call does not execute immediately within the caller's execution context. Instead, the call is internally converted into a message and added to the recipient actor-instance's mailbox for future processing.
#block(
fill: gray.lighten(80%),
inset: 12pt,
radius: 4pt,
stroke: (left: 2pt + gray.lighten(50%))
)[
  *Note on Synchronous Functions:* Pony also supports synchronous functions (`fun`) that can be called outside the actor-instance. In _Coherence_ however, functions are strictly internal and can only be invoked by the actor's own behaviours. They serve exclusively as private helpers to organise logic. All external interaction remains asynchronous.
]

Let us consider a sample program to illustrate these ideas.

```Pony
actor Counter
  var _count: U32
  new create(init: int) =>
    _count = init
  be increment() =>
    _count = _count + 1
  be display(env: Env) =>
    env.out.print("Current count: " + _count.string())

actor Main
  new create(env: Env) =>
    let c = Counter.create(1) // actor-instance creation
    c.increment()
    c.display(env)
```


This example demostrates creating an actor-instance and sending messages to it. Notably, the call to `c.increment()` is non-blocking: the `Main` actor does not stall to wait for the counter to be updated. Despite this asynchronous nature, it is strictly guaranteed that the `display` behaviour call will be processed after the increment. This is ensured by _causal ordering_, a property of the Pony mailbox system which guarantees that messages arrive in causal order (which in this case means the order they were dispatched in).

=== Mailbox and Runtime Characteristics

The Pony runtime associates with each actor-instance a mailbox, which is a FIFO queue of incoming messages. Calling a behaviour simply adds a message to the mailbox. Execution starts from the `create()` constructor of the `Main` actor and ends when all mailboxes are empty.

Whenever a behaviour is called from another actor-instance, the runtime adds information about the particular behaviour call to the queue of the recipient actor-instance and then returns control back to the caller. Note that the control returns only after the message has been added. This is what guarantees causal ordering.

Finally, messages of an actor-instance are processed sequentially---different behaviours directed to the same actor-instance are not run at the same time. The execution of a behaviour is never interrupted (runs to completion). This simplifies implementation as the runtime does not need a mechanism to save and restore the stack of the running behaviour as there is no context switching. This also means that the behaviours can directly utilise the current worker thread's stack. However, this is prone to hogging of worker threads. For this reason, behaviours in Pony are supposed to be short-lived.

== Reference capabilities
Reference capabilities are a novel innovation introduced in Pony. This is what allows Pony to have shared-memory while still preventing data-races.

=== General introduction
Unlike C or C++, Pony does not have raw pointers. Instead, it utilises an object-oriented model similar to Java where data is encapsulated within classes. Instances of the class are stored on the heap and are accessed via references stored on the stack.

To illustrate this, consider the simple account class in Pony.

```
class Account
  var balance: U64
  new create(opening_balance: U64) =>
    balance = opening_balance

actor Main
  new create(env: Env) =>
    // 'acc' is a reference on the stack pointing to 
    // an Account object on the heap.
    let acc: Account ref = Account(100)
    // Members are accessed via the reference
    acc.balance += 50
```

Reference capabilities are a part of the type of every reference. It restricts how the reference can be used. In the example above, an `Account` instance with the capability `ref` is created.

#block(
fill: gray.lighten(80%),
inset: 12pt,
radius: 4pt,
stroke: (left: 2pt + gray.lighten(50%))
)[
*Note on Style:*

In idiomatic Pony, the balance field would typically be private, with mutation restricted to formal methods (like `deposit()`) to maintain encapsulation. However, since Coherence is designed around raw pointers and direct memory access rather than class-based encapsulation, I have opted to access the field directly. This more closely mirrors the style that Coherence aims to facilitate.
]
=== Specific capabilities used in Pony

The main capabilities used in Pony are `iso`, `ref`, `val`, and `tag`. 
(Note: the term _aliases_ in the text below refers to named variables)

- *`iso` (Isolated)*
  - *Permissions:* Read and Write.
  - *Deny Properties:* Denies both read and write access to all other aliases, both locally and globally.
  - *Use Case:* An `iso` reference provides exclusive access to an object. Because the compiler guarantees that no other alias capable of reading or writing to the object exist, the holder can safely mutate the data without worrying about data-races. An `iso` reference can be sent across actor boundaries, provided the sender _consumes_ the alias during the transfer. The compiler prevents the use of a consumed alias.
  - *Code example:*
    ```
    class Data
      var value: String = "Secret"

    actor Main
      new create(env: Env) =>
        // Create an isolated object
        let d_iso: Data iso = Data
        d_iso.value = "Updated" // We can mutate it locally
        // To send it to another actor, we must consume it
        let other = OtherActor.create()
        other.receive_data(consume d_iso)
        // d_iso is now invalid. The following line would be a compile error:
        // env.out.print(d_iso.value)
    ```

- *`ref` (Reference)*
  - *Permissions:* Read and Write.
  - *Deny Properties:* Denies nothing to other aliases within the same actor, but denies all access to aliases in other actors.
  - *Use Case:* This is the standard mutable reference for data confined to a single actor. Multiple `ref` aliases can exist and interact with the same object, but they are prohibited from crossing actor boundaries.
  - *Code example:*
    ```
    actor Main
      new create(env: Env) =>
        let d_ref: Data ref = Data
        // We can create multiple local aliases
        let d_alias: Data ref = d_ref
        d_alias.value = "Local Change" 
        // This would fail to compile because ref is not sendable:
        // let other = OtherActor.create()
        // other.receive_data(d_ref)
    ```
- *`val` (Value)*
  - *Permissions:* Read-only.
  - *Deny Properties:* Denies write access to all other aliases globally.
  - *Use Case:* Since a `val` object is guaranteed to be immutable, it can be shared with an unlimited number of actors simultaneously.
  - *Code example:*
    ```
    actor Main
      new create(env: Env) =>
        let d_val: Data val = Data
        // This would fail to compile as it is not modifiable
        // d_val.value = "dummy"
        // Can create local aliases
        let other_val1 = d_val
        let other_val2 = d_val
        // Safe to share with other actors
        let other_actor = OtherActor.create()
        other_actor.receive_data(d_val)
    ```
- *`tag` (Tag)*
  - *Permissions:* None (can only be compared for equality).
  - *Deny Properties:* Does not deny any aliases.
  - *Use Case:* In Pony, the `tag` capability is primarily used for identifying objects or sending asynchronous messages. Since it allows neither reading nor writing, it can be shared globally without violating isolation. Its usecase in _Coherence_ is less clear, primarily because _Coherence_ does not associate reference capabilities with actor-instances. However, it is still needed for completeness for _viewpoint adaptation_. Viewpoint adaptation will be explored in the next section.

#table(
  columns: (1fr, 1fr, 1fr),
  inset: 10pt,
  align: horizon,
  [*Capability*], [*Local Access*], [*Sendable to Actors?*],
  [`iso`], [Read/Write], [Yes (via consume)],
  [`val`], [Read-only], [Yes],
  [`tag`], [Identity only], [Yes],
  [`ref`], [Read/Write], [No],
)

=== Viewpoint adaptation
Viewpoint adaptation is the mechanism Pony uses to correctly handle references to references. It ensures that the capabilities of an object's fields are always interpreted through the lens of the reference used to access them.

Consider the following code:
```Pony
class A
  var x: U64 = 0

class B
  var a: A ref = A // Field 'a' is mutable within B

actor Main
  new create(env: Env) =>
    let b: B val = B // We have an immutable reference to B
```

What should the capability of `b.a` be? Because `b` has capability `val`, we are permitted to share it across actor boundaries. If `b.a` were allowed to retain its original `ref` capability, a recipient actor could gain mutable access to the object `a`. This would result in two different actors holding a `ref` to the same data simultaneously, directly violating the `ref` invariant and reintroducing data races. The solution to this problem is viewpoint adaptation.

Viewpoint adaptation is a binary operator denoted by the symbol $triangle.r$. Whenever we access a field through a reference, we do not simply get a value of the field's defined type. Instead, we apply $triangle.r$ to combine the capability of the originating reference `O` with the capability of the field `F` to get the resulting capability $O triangle.r F$.

=== Viewpoint Adaptation Table

The following table defines the result of the adaptation operator $O triangle.r F$ in Pony, where $O$ represents the capability of the originating reference and $F$ represents the capability of the field being accessed.

#table(
  columns: (1fr, 1.2fr, 1.2fr, 1.2fr, 1.2fr),
  inset: 10pt,
  align: center + horizon,
  fill: (x, y) => if y == 0 or x == 0 { gray.lighten(80%) },
  stroke: 0.5pt + gray,
  
  [$O triangle.r F$], [`iso`], [`ref`], [`val`], [`tag`],
  [`iso`], [`iso`], [`tag`], [`val`], [`tag`],
  [`ref`], [`iso`], [`ref`], [`val`], [`tag`],
  [`val`], [`val`], [`val`], [`val`], [`tag`],
)

#block(
  fill: gray.lighten(80%),
  inset: 12pt,
  radius: 4pt,
  stroke: (left: 2pt + gray.lighten(50%))
)[
  *Note on Read vs. Write Adaptation:*
  
  In standard Pony, viewpoint adaptation defines the capability of the value produces when reading a field. However, determining whether a field can be written to requires a separate set of _legal write_ rules to prevent safety violations (in addition to considering whether the origin capability is mutable). 
  
  Unlike Pony, _Coherence_ does not require separate rules for reading and writing. Its viewpoint adaptation rules are constructed so that the adapted type naturally prevents safety violations for both operations. For this reason, the additional legal write rules in Pony will not be discussed.
]

== Pony's expressive power
Pony safely reintroduces _shared memory_ using reference capabilities. This ensures data-race freedom at compile time. However, representing patterns where multiple actors frequently modify a shared data structure remains awkward. Two general patterns predominate:

=== The Manager pattern
This pattern encapsulates data within a _Manager_ actor. All other actors interact with the data by sending asynchronous requests to the Manager.

```Pony
actor IntManager1
  var _data: U64 = 0
  // The 'get' behaviour takes in a callback
  be get(cb: {(U64): None} val) =>
    cb(_data)
  be update(value: U64) =>
    _data = value
```
In the code above, threads asynchronously send update or query requests as messages. For querying, the `IntManager1` is given a sendable callback to which it sends the current value of `_data`.

#block(
  fill: gray.lighten(80%),
  inset: 12pt,
  radius: 4pt,
  stroke: (left: 2pt + gray.lighten(50%))
)[
  *Sendable callbacks:*
  A _sendable_ callback is a first-class function that can be dispatched to other actors and executed within their respective execution contexts without the risk of data-races. Since _Coherence_ does not implement first-class functions, the specific type-checking rules governing sendable closures will not be discussed. The provided code serves only as a conceptual illustration of the manager pattern.
]

Let us consider the problem of swapping two shared integers.

```Pony
actor Swapper1
  be swap(m1: IntManager1, m2: IntManager1) =>
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
- This pattern fails to provide atomicity. For example, between Step 2 and Step 3, a third actor could update `m1` and `m2`. The swapper would then overwrite that new value with stale data.

=== Hot-Potato Model
To solve the atomicity problem, developers can use the _hot-potato_ model. Here, the manager transfers exclusive ownership of the object to the requester using the `iso` reference capability. The ownership is returned when the requester is done with the task.

```Pony
// A simple shared container to hold our value
class IntContainer
  var value: U64 = 0
  new create(v: U64) => value = v

actor IntManager2
  var _data: (IntContainer iso | None)

  new create(v: IntContainer iso) => _data = consume v

  // Taking the potato: the manager gives up the reference
  be take(cb: {(IntContainer iso): None} val) =>
    // In Pony, destructive writes return the overwritten value
    var potato: (IntContainer iso | None) = (_data = None)
    if potato is (let p: IntContainer iso) then
      cb(consume p)

  // Returning the potato
  be give(p: IntContainer iso) =>
    _data = consume p  
```

This code can be made more robust by ensuring that the `take` operation never fails. If `IntManager2` is currently empty (because another actor is holding the potato), it stores the incoming callback in a FIFO queue. When the potato is eventually returned via `give`, the manager automatically dispatches it to the next callback in the queue.

Assumming this more robust `IntManager` (call this `IntManager3`), the code to swap the two shared integers can be written as:

```Pony
actor Swapper2
  be swap(m1: IntManager3, m2: IntManager3) =>
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
- The system relies entirely on the requester to return the reference via the `give` behaviour. If a programmer forgets to return the _potato_, the resource is effectively lost, and all other actors in the queue are blocked indefinitely.
- A more fundamental flaw is the indirect introduction of deadlocks. Because ownership is acquired sequentially across multiple actors, the system is susceptible to the _circular wait_ condition.
  Consider two actors attempting to access the same two managers simultaneously:
  - Actor A calls `m1.take()` and successfully receives `p1`. It then calls `m2.take()` and enters `m2`'s wait queue.
  - Actor B calls `m2.take()` and successfully receives `p2`. It then calls `m1.take()` and enters `m1`'s wait queue.

  Now, Actor A is waiting for Actor B to call `m2.give()`, and Actor B is waiting for Actor A to call `m1.give()`. Neither can proceed, and the system is stuck. Pony programs terminate when there are no more messages to run. So in this case, the program simply terminates prematurely.


#block(
fill: gray.lighten(80%),
inset: 15pt,
radius: 5pt,
stroke: (left: 3pt + gray)
)[
 *Motivation*

The primary goal of _Coherence_ is to bridge the gap between the safety of the Actor model and the expressive power of imperative programming.

A developer should not have to manage nested callbacks or manual potato queues just to perform a swap. Instead, they should be able to express the operation as a single, atomic unit of work where the compiler handles the underlying synchronisation. The language we are striving towards should allow programmers to do something like:

```
atomic {
  var temp: int = *a;
  *a = *b;
  *b = temp;
}
```
]

== Safe locking
As demonstrated above, there are coordination patterns that are hard to implement without locks. _Coherence_ aims to reintroduce locks while maintaining deadlock-freedom. This section describes some concepts needed for this.

=== Locked reference capability
The standard solution to shared-memory is to protect the shared datastructure with a lock, making sure that only one concurrent unit is accessing it at the same time. How can we incorporate this idea in the reference capability model?/*Add a reference here about kappa*/ The Kappa type system by Elias Castegren@kappa_system does this using the `locked` reference capability. An object with this capability comes with an implicit lock. Whenever a method of the object is called, the lock associated with the object is taken, which is released after the method execution completes. This object can be shared between actors, and hence allows for easy sharing of mutable state.

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
- Atomicity is still not achieved. As locks are released after every operation, it is still possible for another actor to modify the shared integer values between the read and the write.
- In some contrived examples, deadlocks can still occur. This occurs when object fields contain locked references.

  ```Pony
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

=== Atomic sections and lock-set inference
An atomic section is a programming construct that ensures a block of code executes as an indivisible, uninterrupted operation. In _Coherence_, we want the programmers to be able to wrap code in atomic sections without any manual taking of locks. The compiler should automatically infer and acquire the necessary locks to guarantee data-race and deadlock free execution.

_Coherence_ draws inspiration from Gudka work on automatic lock-inference for Java/*Insert reference here*/. Gudka's system _Lockguard_@gudka_thesis translates Java code containing atomic sections to an equivalent program that implements these atomic sections using locks. _Lockguard_ performs a lock-set inference on an atomic section to determine the locks needed, and then generates code to take these locks at the start of the atomic section and release them at the end.

_Coherence_ protects shared memory using a `locked<LOCK_NAME>` reference capability. Unlike Kappa, programmers are expected to label the locks that protect a particular pointer (the locks are explicit). The idea is that a pointer can only be dereferenced once its associated lock has been acquired. However, _Coherence_ does not allow programmers to manually acquire locks (as they may write themselves into deadlocks).  A pointer with a `locked` capability can only be dereferenced within an atomic section. Just like _Lockguard_, the _Coherence_ compiler performs a lock-set inference to get the set of locks needed to protect all pointers in the section. It then acquires them in a canonical order to prevent deadlocks.

Hence, we can finally write the code that we were striving towards!
```
actor Swapper
  be swap(ptr1: int locked<A>, ptr2: int locked<B>) =>
    // The compiler infers that both lock A and lock B are required.
    // They are acquired in a globally fixed order (e.g., A then B) 
    // before the code below executes.
    atomic {
      let temp = *ptr1
      *ptr1 = *ptr2
      *ptr2 = temp
    }
    // Both locks are released here.
```
== Compilers
For the purpose of this project, a compiler consists of a frontend (lexer and parser), a validation phase, and a code-generation phase. Languages typically also include a runtime, which is prewritten code linked with the compiled output to provide environmental support like memory management and scheduling. In _Coherence_, the runtime is critical. It manages actor mailboxes, source-level locks and actor scheduling.

#figure(
  block(
    fill: gray.lighten(95%),
    inset: 15pt,
    radius: 8pt,
    stroke: 0.5pt + gray.lighten(50%),
    grid(
      columns: (100pt, auto, 100pt, auto, 100pt), // Fixed column widths for symmetry
      align: horizon + center,
      gutter: 12pt,
      
      // Block 1: Frontend
      stack(dir: ttb, spacing: 8pt,
        rect(width: 100pt, height: 50pt, radius: 4pt, fill: blue.lighten(90%), stroke: 1pt + blue, 
          align(center + horizon)[*Frontend*]
        ),
      ),
      
      text(size: 15pt, sym.arrow.r),
      
      // Block 2: Validation
      stack(dir: ttb, spacing: 8pt,
        rect(width: 100pt, height: 50pt, radius: 4pt, fill: orange.lighten(90%), stroke: 1pt + orange, 
          align(center + horizon)[*AST Validation*]
        ),
      ),
      
      text(size: 15pt, sym.arrow.r),
      
      // Block 3: Codegen
      stack(dir: ttb, spacing: 8pt,
        rect(width: 100pt, height: 50pt, radius: 4pt, fill: green.lighten(90%), stroke: 1pt + green, 
          align(center + horizon)[*IR Generation*]
        ),
      )
    )
  ),
  caption: [The _Coherence_ Compiler Pipeline],
)

#figure(
  block(
    fill: gray.lighten(97%),
    inset: 15pt,
    radius: 8pt,
    stroke: 0.5pt + gray.lighten(50%),
    grid(
      columns: (60pt, 30pt, 100pt, 30pt, 100pt, 30pt, 70pt),
      column-gutter: 4pt,
      row-gutter: 8pt,
      align: center + horizon,

      // Row 1: The Runtime input (floating above the Linker)
      [], [], [], [], 
      rect(width: 65pt, height: 30pt, stroke: 0.5pt, fill: white, [Runtime]), 
      [], [],

      // Row 2: The actual flow connectors for the Runtime
      [], [], [], [], 
      text(size: 10pt, sym.arrow.b), 
      [], [],

      // Row 3: The main Pipeline
      // 1. Source
      rect(width: 60pt, height: 30pt, stroke: 0.5pt, fill: white, [Source]),
      
      // Arrow
      text(size: 15pt, sym.arrow.r),

      // 2. Coherence Black Box
      rect(
        width: 100pt, 
        height: 50pt, 
        radius: 4pt, 
        fill: purple.lighten(95%), 
        stroke: 2pt + purple, 
        align(center + horizon)[
          #stack(dir: ttb, spacing: 2pt,
            strong("Coherence"),
          )
        ]
      ),

      // Arrow
      text(size: 15pt, sym.arrow.r),

      // 3. LLVM & Linker
      rect(
        width: 100pt, 
        height: 50pt, 
        radius: 4pt, 
        fill: red.lighten(95%), 
        stroke: 1pt + red, 
        align(center + horizon, strong("LLVM lowering and linking"))
      ),

      // Arrow
      text(size: 15pt, sym.arrow.r),

      // 4. Executable
      rect(width: 65pt, height: 30pt, stroke: 0.5pt, fill: white, [Executable])
    )
  ),
  caption: [Integration of the _Coherence_ compiler and runtime]
)


=== Runtime

The runtime for _Coherence_ needs to do the following:

- The runtime maintains a global _ready queue_ of actor-instances prepared for execution. Each actor-instance has a mailbox, which contain all the pending behaviour calls. A fixed pool of worker threads pick actors from the ready queue and execute their behaviours.

- The actor-instances can try to acquire locks. When an instance is unable to acquire a lock, we do not want the entire worker thread to be waiting. Instead, we would want the worker thread to be able to put the actor-instance in a wait queue and move on to run other instances.

- The source-level code can perform actions that would modify the runtime datastructures (e.g. creating an actor-instance, calling a behaviour of an actor, trying to acquire a lock). These actions are referred to as _priviledged_. The compiled source-level code performs these _priviledged_ actions by calling functions defined in the runtime (referred to as _traps_). Hence, it must be easy for the compiled code to interoperate with the runtime.

The _Coherence_ runtime is implemented in C++20. This choice was driven by the need for low-level control over system resources. In the following section, we will explore the specific features that C++ facilitates. 

==== Fine-grained concurrency
The C++ standard library provide many types for fine-grained concurrency. `std::thread` provides direct access to OS-level threads, `std::mutex` provides access to OS-level locks, and `std::atomic` allows for lock-free atomic access to simple data types. These tools are essential for building the high-performance, thread-safe datastructures that the runtime requires.

==== LLVM compatibility
The _Coherence_ compiler compiles the source code to _LLVM IR_. LLVM IR is a low-level, hardware independent intermediate representation. LLVM IR organises code into functions, which adhere to the C _Application Binary Interface_ (ABI).
#block(
fill: gray.lighten(80%),
inset: 12pt,
radius: 4pt,
stroke: (left: 2pt + gray.lighten(50%))
)[
  *ABI*

  An ABI is a set of low-level rules governing how compiled entities interact at the machine level. The ABI defines:
  - *Calling Conventions:* Which CPU registers or stack locations are used to pass function arguments and return values.
  - *Data Layout:* The size, alignment, and padding of data structures in memory.
]
C++ functions can also be exported using this same ABI (via the `extern "C"` linkage). This allows the runtime and the generated LLVM code to freely interoperate with each other.

==== User-level context-switching
Unlike Pony, _Coherence_ behaviours have the ability to pause execution before completion.
```
be dummy_behaviour {
  var a: int locked<A>;
  atomic {
    *a; // Dereferencing [a] requires access to lock [A].
  }
}
```
In the code above, if `dummy_behaviour` reaches the atomic section and realises that the required lock (in this case `A`) is held by another actor-instance, it must wait until the lock is available. The runtime should not be idle until the lock is available. It should be able to move on to running other actor-instances. When the lock is available again, the runtime should be able to resume the execution of the behaviour seamlessly.

One naive solution to this requirement is _1:1 scheduling_. 1:1 scheduling is where each unit of concurrency (in _Coherence_, this is the running behaviour), is mapped directly to its own OS-level thread. Under this model, utilizing standard OS-level mutexes to implement source-level locks would solve the suspension and resumption issues automatically, as the operating system would handle blocking the thread and resuming it once the lock becomes available. However, this approach has the following drawbacks:

- *Context-switching overheads*: Every suspension triggers a costly context-switch. This creates a performance bottleneck when actors yield or block frequently. 
- *Threads are not lightweight*: _Coherence_ aims for lightweight concurrency, where one can easily spawn thousands of actor-instances without issues. Standard OS threads are ill-suited for this scale. OS threads typically reserve large, fixed-size stacks, which will waste memory. Also, kernel schedulers are not optimized for tens of thousands of threads. As the thread count increases, scheduling efficiency degrades, eventually leading to thread exhaustion.

Hence, I choose to implement _M:N scheduling_, where a fixed pool of N worker threads multiplexes the execution of M actor behaviours. A critical consequence of this M:N architecture is that behaviours cannot execute directly on the worker thread's native stack. If a behaviour were to use the worker thread's stack, suspending it would require the runtime to copy the specific segment of the stack corresponding to the behaviour's execution context elsewhere so execution can be restored later. This is clearly pretty inefficient. The runtime must instead provide each behaviour with its own independent stack, and implement a mechanism for the worker thread to swap between different stacks. To implement this stack-swapping mechanism, the _Coherence_ runtime utilises the `boost` library's `fcontext_t` type (`boost::context::detail::fcontext_t`). This is a low-level, assembly-based primitive that facilitates highly efficient context switching entirely in user-space.


```cpp
namespace ctx = boost::context::detail;
void actor_logic(ctx::transfer_t t) {
    std::cout << "Actor: Started. Running on a private stack\n";
    // Suspend the actor and return control to the worker thread.
    // The second argument allows passing data back to the scheduler.
    void* suspend_msg/* = ... (construct the appropriate message)*/;
    t = ctx::jump_fcontext(t.fctx, suspend_msg);
    // Control returns to the actor
    std::cout << "Actor: Resumed!\n";
    // Final context switch to return control to the runtime upon completion.
    suspend_msg/* = ... (construct the appropriate message)*/;
    ctx::jump_fcontext(t.fctx, suspend_msg);
}

int main() {
    void* stack_mem = std::malloc(64 * 1024); // Allocate 64KB stack
    // Prepare the execution context at the top of the allocated memory.
    ctx::fcontext_t actor_ctx = ctx::make_fcontext(
        static_cast<char*>(stack_mem) + 64 * 1024, 64 * 1024, actor_logic
    );
    std::cout << "Main: Switching to actor...\n";
    // Transfer control to the actor. This call suspends [Main].
    ctx::transfer_t t = ctx::jump_fcontext(actor_ctx, nullptr);

    // Main resumes here once the actor calls jump_fcontext to yield.
    std::cout << "Main: The actor yielded.\n";
    // Upon resumption, the worker thread can analyze [t.data] (the suspend_reason) 
    // to determine the reason of suspension of the actor. When the worker thread
    // decides to resume execution of the waiting actor, it can simply context switch
    // again
    ctx::jump_fcontext(t.fctx, nullptr);
    std::cout << "Main: Actor finished.\n";
    return 0;
}
```

The above code is a simple demonstration of how one might go about implementing the runtime scheduler using `fcontext_t`.
- `fcontext_t` is a handle representing the snapshot of the processor's registers before the context switch.
- `make_fcontext` intialises a new `fcontext_t` by taking a pointer to the top of a pre-allocated memory block and a function pointer for the entry point. It sets up the initial state of this stack so that the very first time the runtime switches to it, the CPU begins executing the behaviour's logic as if it were a standard, fresh function call.
-  `jump_fcontext` performs the actual context switch by swapping the CPU's current registers with those of the target context. It passes a `transfer_t` object as a parameter to the target function, which contains the context of the caller so that the callee knows exactly where to return control to when it needs to yield or finish.

=== Intermediate representation
As mentioned earlier, we choose to compile _Coherence_ to LLVM IR. This approach offers several advantages:
- *Architecture independence*: By targeting LLVM, the _Coherence_ compiler remains platform-agnostic (the lowering to machine-specific assembly is handled by the LLVM backend).
- *Out-of-box optimizations*: LLVM implements a vast amount of optimization passes. Compiling down to LLVM means that the _Coherence_ compiler gets these optimizations for free.


=== Language choice for the compiler
I chose C++20 as the primary language for developing the _Coherence_ compiler. The initial decision was driven by my familiarity with the language and its robust support for generating LLVM IR. However, the project's evolution revealed challenges I had not previously considered. These are sketched out in the (TODO: WILL BE FILLED LATER) chapter. In retrospect, a functional language like OCaml might have been better suited to this task. An unforseen advantage of using C++ was that since the _Coherence_ runtime was written in C++, keeping the compiler in the same language allowed for a unified build system (CMake) and a shared testing infrastructure.

== Requirement analysis
The development of _Coherence_ is guided by the following requirements.

- *Static Safety*: Devise a type system and implement a type checker that strictly enforces data-race and deadlock freedom.

- *Compiler and Runtime*: Implement a working end-to-end compiler capabable of lowering _Coherence_ source code to LLVM IR, and a runtime that satisfies the needs of the language.

- *Expressive power*: The language must be sufficiently expressive to allow programmers to implement diverse concurrent algorithms without the syntax becoming cumbersome.


== Software engineering techniques
A number of software tools and techniques were used to ensure that the project progressed smoothly.
=== Build system
I used _CMake_ as the project’s build system. CMake has good integration with VSCode (my IDE) which allowed me to build my project and run tests with a single click.

=== Version control system
I utilised _Git_ for version control. Alot of the implementation choices required significant trial and error. Using Git allowed me to explore new implementation ideas and refactor core logic with the safety net of being able to revert to previous stable states. To safeguard against hardware failure, the repository was hosted remotely on GitHub.

=== Development model
The development of Coherence followed the spiral model. This approach was essential for managing the technical risks inherent in building a custom type system and a low-level runtime. Instead of attempting to implement the full language specification at once, I proceeded through the following stages:

// TODO: If I end up adding templated locks, add that in this initial core cycle part
- *Initial Core Cycle*: The first iteration focused on implementing a bare-bones version of the language. I implemented a restricted syntax that excluded high-level complexities such as lock-inference for mutually recursive functions or viewpoint adaptation for references to references. The goal of this phase was to establish a functional end-to-end compiler, ensuring that the skeleton of the system was operational. This provided a working foundation early in the project, allowing me to verify the core architecture before committing to the full implementation of more difficult language features.

- *Feature-Driven Iterations*: Once the core was stable, I performed subsequent cycles to iteratively expand the language's capabilities.

- *Incremental verification*: At the end of each cycle, I wrote sample programs using these new features and added tests to make sure that the features worked as intended.

== Starting point
At the start of my project, I had a solid foundation at the Part IB level in Compilers, Concurrency, Operating Systems and C++. I had some familiarity with existing literature on type systems for safe concurrent programming (e.g. Pony, Kappa, Rust). I also had some prior hands-on experience using lexer and parser generators. No code existed at the start of the project.

#bibliography("../bibliography.bib", style: "ieee")