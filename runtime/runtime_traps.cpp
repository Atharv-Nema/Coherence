#include "runtime_traps.hpp"
#include <cassert>
#include <atomic>
#include <iostream>
#include <syncstream>

void print_int(int i) {
    std::osyncstream(std::cout) << i << "\n";
}

void handle_unlock(std::uint64_t lock_id) {
    assert(runtime_ds->mutex_map.find(lock_id) != runtime_ds->mutex_map.end());
    UserMutex& mutex = (runtime_ds->mutex_map)[lock_id];
    mutex.unlock(runtime_ds);
}

void handle_behaviour_call(
    uint64_t instance_id,
    void* message,
    void (*behaviour_fn)(void*)
) {
    using State = ActorInstanceState::State;
    auto actor_instance_opt = runtime_ds->id_actor_instance_map.get_value(instance_id);
    assert(actor_instance_opt != std::nullopt);
    std::shared_ptr<ActorInstanceState> actor_instance = *actor_instance_opt;
    State expected = State::EMPTY;
    actor_instance->mailbox.emplace_back(MailboxItem { instance_id, message, behaviour_fn });
    if(actor_instance->state.compare_exchange_strong(expected, State::RUNNABLE)) {
        // We are doing compare_exchange_strong so that actor_instance is added only once
        runtime_ds->schedule_queue.emplace_back(instance_id);
        runtime_ds->thread_bed.release();
    }
}

void* get_instance_struct(uint64_t instance_id) {
    auto actor_instance_opt = runtime_ds->id_actor_instance_map.get_value(instance_id);
    assert(actor_instance_opt != std::nullopt);
    auto actor_instance = *actor_instance_opt;
    return actor_instance->llvm_actor_object; 
} 


// Called by LLVM right after allocating actor memory.
// Only registers the actor and returns its unique instance_id.
// Constructor runs synchronously in the caller, not the actor.
// CR: Slightly misleading name
std::uint64_t handle_actor_creation(void* llvm_actor_object)
{
    std::uint64_t instance_id = ++(runtime_ds->instances_created);

    auto state = std::make_shared<ActorInstanceState>(llvm_actor_object, instance_id);

    runtime_ds->id_actor_instance_map.insert(instance_id, state);
    return instance_id;
}

void suspend_instance(uint64_t actor_instance_id, void* suspend_tag) {
    auto actor_instance_opt = runtime_ds->id_actor_instance_map.get_value(actor_instance_id);
    assert(actor_instance_opt != std::nullopt);
    auto actor_instance = *actor_instance_opt;
    boost_ctx::fcontext_t main_ctx = actor_instance->next_continuation;
    boost_ctx::transfer_t t = boost_ctx::jump_fcontext(main_ctx, suspend_tag);
    // Note that here t.data is ignored. This is filled with the behaviour parameter, but it does not have
    // to be. (this path is followed for eg when it asks the runtime for lock acquisition and it succeeds).
    actor_instance->next_continuation = t.fctx;
}