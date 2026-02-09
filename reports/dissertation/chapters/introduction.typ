#set text(size: 12pt, font: "Linux Libertine")
#set heading(numbering: "1.1")
#set page(paper: "a4", margin: 2cm)
#set par(justify: true)

= Introduction

The trajectory of processor development has shifted fundamentally in recent decades. Due to physical limitations, the era of rapid single-core performance gains effectively ended. In response, manufacturers pivoted towards multi-core architectures. Software performance is no longer a "free lunch" provided by hardware improvements. To utilize modern processors to their greatest extent, writing concurrent code is essential.

However, moving from a sequential to a concurrent execution model introduces a significant cognitive overhead. Managing multiple threads requires a departure from the intuitive, linear flow of traditional programming. This complexity leads to various hard-to-catch concurrency errors, which are notoriously subtle and difficult to reproduce.

This project implements and evaluates a compiler for a language named _Coherence_, which aims to alleviate some of the fundamental issues faced while writing concurrent code.


== Motivation

While writing concurrent code, developers frequently encounter two types of errors: _data-races_ and _deadlocks_.

A data-race occurs when two or more threads attempt to access the same memory location simultaneously, and at least one of those accesses is a write. Without synchronization, the final state of the memory becomes dependent on the unpredictable timing of the processor's scheduler. This results in _undefined behavior_---a state where the program's output is no longer predictable.

The most common defense against data races is the use of a _lock_. A lock is a synchronization primitive that grants a thread exclusive access to a resource. By "locking" a piece of data, a thread ensures that no other thread can modify it until the lock is released. However, without a robust locking paradigm, programmers can encounter _deadlocks_. A deadlock occurs when two or more threads enter a circular dependency, where each thread holds a lock that another thread needs to proceed. Because no thread can move forward or release its currently held lock, the entire system reaches a state of permanent stagnation.

One programming model that circumvents these problems is the _actor model_. In this paradigm, the _actor_ is the fundamental unit of concurrency. Unlike traditional threads, actors do not share memory with each other. Instead, each actor manages a private state and communicates with other actors exclusively through asynchronous message passing. Each actor possesses an internal _mailbox_ that stores incoming messages. These messages are processed sequentially by the actor. Because there is no interleaving of execution within an individual actor and no state is shared between them, the model avoids both data races and deadlocks.

However, this pure actor model has several real costs. Copying data between actors can be expensive, and coordination patterns that are naturally expressed with locks become awkward or incur extra latency. In short, forbidding shared memory and locks entirely simplifies correctness but can hinder performance and expressiveness.

This project aims to demonstrate that safety does not need to come at the cost of performance and expressiveness. _Coherence_ is a language designed to bridge this gap, aiming to retain the safety and simplicity of the actor model while safely reintroducing shared memory and locks.

== Related Work
(Note: throughout this document, the terms _pointer_ and _reference_ will be used interchangeably)

_Coherence_ builds upon several related works.

=== The Pony Programming Language
The _Pony_ programming language successfully reintroduced shared-memory into the actor model without sacrificing safety. The core innovation of Pony was its system of _reference capabilities_.

In Pony, every _reference_ type has an associated capability that restricts how the reference can be shared between actors. For example, the isolated (iso) reference capability guarantees that if a variable holds the address to a memory location, no other variable in the entire program holds that same address. This allows a reference to be safely transferred between actors without the overhead of copying, provided that the sending actor 'consumes' the variable originally storing the reference, relinquishing access to maintain the single-pointer guarantee. The programmer assigns these capabilities to pointers they use, and the pony compiler uses these capabilities to statically rule out programs that may lead to data-races.

=== Gudka's Lock Inference for Java
_Lock inference_ is a term used to refer to program analysis techniques that automatically determine which locks must be acquired to ensure a block of code executes safely. By shifting the responsibility of locking from the developer to the compiler, lock inference eliminates human errors that typically lead to data races and deadlocks.

In this work, Gudka developed a system of lock-inference for Java. In his model, developers are no longer required to manage individual locks. Instead, they wrap sensitive code in _atomic sections_---logical units of code that the system guarantees will execute without interference from other threads. Gudka's system statically analyses these sections to compute the _lock-set_---the collection of locks required to protect the data being accessed. To prevent deadlocks, the system uses a "try-and-back-off" strategy, ensuring that if all locks cannot be acquired safely, the current ones are released before retrying.

=== Castegren's Oolong
Castegren introduced a new reference capability called `locked` in his work on Oolong. A reference protected by this capability can only be accessed after the lock associated with the capability has been acquired, preventing data-races.

== Project Objectives

The primary objective of this project is to design and implement a compiler for _Coherence_. This involves the following:

+ Implement a _type-checker_ that enforces that all accepted programs are data race and deadlock free.
+ Build a functional compiler that parses _Coherence_ source code and translates it into an efficient low-level representation for execution. 
+ Empirically demonstrate the compiler's correctness and expressive power by compiling and executing a representative suite of concurrent programs.