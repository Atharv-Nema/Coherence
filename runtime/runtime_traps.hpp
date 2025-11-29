#pragma once
#include "runtime_datastructures.hpp"

extern RuntimeDS* runtime_ds;

// Non interrupting traps (called directly from LLVM)
extern "C" {
    enum SuspendTagKind: uint32_t {
        RETURN = 0,
        LOCK   = 1
    };
    struct SuspendTag {
        SuspendTagKind kind;
        uint64_t lock_id;
    };

    void handle_unlock(uint64_t lock_id);
    void handle_behaviour_call(
        uint64_t instance_id, 
        void* message,
        void* frame,
        SuspendTag (*resume_fn)(void*),
        void (*destroy_fn)(void*)
    );
    /* 
    It is llvm's responsibility to allocate space for the actor instance. It passes it
    onto the runtime to get an [instance_id] and forgets about the pointer to the 
    underlying object.
    The control flow is: 
    1. LLVM code allocates space for actor members
    2. Register the actor using [handle_actor_creation] to get an instance_id
    3. LLVM calls the constructor
    4. LLVM forgets the pointer to the llvm_actor_object 
    */
    std::uint64_t handle_actor_creation(void* llvm_actor_object);

    // Interrupting traps (called indirectly from the runtime)
    // Returns true if lock has been successfully acquired, false if it is waiting
    bool handle_lock(uint64_t actor_instance_id, uint64_t lock_id);
}