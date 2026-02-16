#set text(size: 12pt, font: "Linux Libertine")
#set heading(numbering: "1.1")
#set page(paper: "a4", margin: 2cm)
#set par(justify: true)

= Preparation

This chapter details the preparatory work undertaken before the development of _Coherence_.

This chapter contains:

- *Actor model:* A detailed analysis of the actor-based concurrency model used in Pony, which heavily inspires the actor model used in _Coherence_
- *Reference capabilities:* An analysis of the  _reference capabilities_ used in Pony to rule of data races.
- *Compiler overview*: An overview of the compilation pipeline alongside the runtime components necessary for a language like _Coherence_.
- *Techinical infrastructure*: An analysis of the tools, libraries and specific language features leveraged to realize the final implementation
- *Project Definition:* A structured requirement analysis and a description of the starting point of the project

== Actor Model in Pony
// <An introductory statement about actors>
// 1. Overview of actors (syntax and basic semantics)
// 2. Mailbox semantics
// 3. Runtime overview

The Pony programming language is built upon the actor model. However, Pony introduces specific variations to the pure actor model described in the introduction to better support high-performance programming. This section explores Pony's actor model in detail.

=== Basic syntax and semantics
In pony, an _actor_ is actually a template to print out actors (just like a _class_ is a template to print out _objects_). In the rest of the document, I would sometimes refer to actors as the template and sometimes as the specific instances when the meaning is not ambiguous. In ambiguous situations, I would use the terms _actor-template_ and _actor-instance_.

Actors in pony are a collection of state and its behaviours. An example of an actor-template is illustrated below:

```pony
actor Greeter
  var _name: String // State

  new create(name: String) => // Constructors
    _name = name

  be say_hello() => // Behaviours
    env.out.print("Hello, " + _name)
```

A Pony actor consists of three primary components:

*State:* Private data stored within the actor-instance. This state is strictly isolated: it cannot be accessed directly by any other actor-instance.
*Constructors (`new`):* These are functions used to initialize the state of an actor instance upon creation.
*Behaviors (`be`):* These are the asynchronous interface through which actors communicate. Unlike standard synchronous functions, a behavior call does not execute immediately within the caller's execution context. Instead, the call is transparently converted into a message and dispatched to the recipient actor-instance's mailbox for future processing.

#quote(block: true)[
  *Note on Synchronous Functions:* Pony also supports synchronous functions (`fun`) that can be called outside the actor-instance. In _Coherence_ however, functions are strictly internal and can only be invoked by the actor's own behaviors. They serve exclusively as private helpers to organize logic, ensuring that all external interaction remains asynchronous.
]

Let us consider another program to illustrate these ideas.

```pony
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


This example demostrates creating an actor and sending messages to it. Notably, the call to `c.increment()` is non-blocking: the `Main` actor does not stall to wait for the counter to be updated. Despite this asynchronous nature, it is strictly guaranteed that the `display` behavior will reflect the increment. This is ensured by _causal ordering_, a property of the Pony mailbox system which guarantees that messages arrive in causal order (which in this case means the order they were dispatched in). Causal ordering is explored in further details later in this chapter.


=== Mailbox and Runtime Characteristics

// TODO: Add information about causal message ordering and probably add images and stuffS

This sub-section deals more deeply with the inner details of the implementation of Pony's actor model, which is useful to understand for the implementation of _Coherence_.

The Pony runtime associates with each actor-instance a mailbox, which is a FIFO queue of incoming messages. Actor-instances at the source-level are just identifiers of the particular actor instance, and calling a behaviour simply adds a message to the mailbox. Execution starts from the `create()` constructor of the `Main` actor and ends when all mailboxes are empty.

Whenever a behaviour is called from another actor-instance, the runtime adds information about the particular behaviour call to the queue of the callee actor-instance and then returns control back to the caller. Note that the control returns only *after* the message has been added. This is what guarantees causal ordering. 

Finally, messages of an actor-instance are processed sequentially and executions of different behaviours directed to the same actor-instance are not run at the same time. While running a particular behaviour, the execution is never interrupted (runs to completion). This simplifies implementation as the runtime does not need to save or restore the stack state (context switching), and it can directly utilize the current worker thread's stack. However, this is prone to hogging of worker threads. Behaviours in Pony are supposed to be short-lived for this reason.


== Reference capabilities
Reference capabilities are a novel innovation introduced in Pony. This is what allows Pony to use shared-memory while still preventing data-races.

=== General introduction
Unlike C or C++, Pony does not have raw pointers. Instead, it utilizes an object-oriented model similar to Java, where data is encapsulated within classes. Instances of the class are stored on the heap and accessed via references stored on the stack.

Consider the simple account class in Pony

```
class Account
  var balance: U64
  
  new create(opening_balance: U64) =>
    balance = opening_balance
```

We can create an instance on this `Account` class

```
actor Main
  new create(env: Env) =>
    // 'acc' is a reference on the stack pointing to 
    // an Account object on the heap.
    let acc: Account ref = Account(100)
    
    // Members are accessed via the reference
    acc.balance += 50
```

A reference capability is a component associated with the type of every reference. In the example above, an `Account` instance with the capability `ref` has been created. This restricts how `Account` can be used.

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
(Note: aliases in the text below are just named variables)

- *`iso` (Isolated)*
  - *Permissions:* Read and Write.
  - *Deny Properties:* Denies both read and write access to all other aliases, both locally and globally.
  - *Use Case:* An `iso` reference provides exclusive access to an object. Because the compiler guarantees that no other aliases capable of reading or writing to that memory exist, the holder can safely mutate the data without worrying about data-races. An `iso` reference can be sent across actor boundaries, provided the sender _consumes_ the alias during the transfer. The compiler prevents the use of a consumed value.
  - *Code example:*
    ```
    class Data
      var value: String = "Secret"

    actor Main
      new create(env: Env) =>
        // Create an isolated object
        let d_iso: Data iso = Data
        d_iso.value = "Updated" // We can mutate it locally
        
        // To send it to another actor, we must 'consume' it
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
        // 'val' is the default for string literals
        let d_val: Data val = Data
        // This would fail to compile as it is not modifyable
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
  - *Use Case:* In Pony, the `tag` capability is primarily used for identifying objects or sending asynchronous messages. Since it allows neither reading nor writing, it can be shared globally without violating isolation. Its usecase in _Coherence_ is less clear, primarily because _Coherence_ does not associate reference capabilities with actor-instances. However, it is still needed for completeness for _viewpoint adaptation_. _Viewpoint adaptation_ will be explored in later sections.

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
Viewpoint adaptation is the mechanism Pony uses to correctly handle _references to references_. It ensures that the capabilities of an object's fields are always interpreted through the lens of the reference used to access them.

Consider the following code:
```pony
class A
  var x: U64 = 0

class B
  var a: A ref = A // Field 'a' is mutable within B

actor Main
  new create(env: Env) =>
    let b: B val = B // We have an immutable reference to B
```

What should the capability of `b.a` be? Because `b` is a `val`, we are permitted to share it across actor boundaries. If `b.a` were allowed to retain its original `ref` capability, a recipient actor would gain mutable access to the object `a`. This would result in two different actors holding a `ref` to the same data simultaneously, directly violating the `ref` invariant and reintroducing data races.

The solution to this problem is viewpoint adaptation.

Viewpoint adaptation is a binary operator denoted by the symbol $triangle.r$. Whenever we access a field through a reference, we do not simply take the field's defined type. Instead, we apply this operator to combine the capability of the originating reference `O` with the capability of the field `F` to get the resulting capability $O triangle.r F$.

The viewpoint adaptation operator outputs are listed below:
#let vpt = $triangle.r$

=== Viewpoint Adaptation Table

The following table defines the result of the adaptation operator $O vpt F$, where $O$ represents the capability of the originating reference and $F$ represents the capability of the field being accessed.

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


// <Introduce the locked re
// Introduce Pony's reference capabilities and explain the concepts with examples
// Then introduce the locked reference capability from Kappa.

// === Viewpoint adaptation
// <Explain the need for viewpoint adaptation with an example>
// <Give the table for pony>
// <Incorporate the rules with locked>

== Advanced C++ features
1. Language choice for the compiler
I think that this was the wrong choice.
I guess I would include the parser used

== LLVM

== Lock-set inference

== Software engineering techniques

== Requirement analysis

== Starting point
