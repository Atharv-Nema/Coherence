#include "runtime_traps.hpp"
#include <cassert>
#include <atomic>

void handle_unlock(std::uint64_t lock_id) {
    assert(runtime_ds->mutex_map.find(lock_id) != runtime_ds->mutex_map.end());
    UserMutex& mutex = (runtime_ds->mutex_map)[lock_id];
    mutex.unlock(runtime_ds);
}

void handle_behaviour_call(
    uint64_t instance_id,
    void* message,
    void* frame,
    void (*behaviour_fn)(void*)
) {
    using State = ActorInstanceState::State;
    auto actor_instance_opt = runtime_ds->id_actor_instance_map.get_value(instance_id);
    assert(actor_instance_opt != std::nullopt);
    std::shared_ptr<ActorInstanceState> actor_instance = *actor_instance_opt;
    State expected = State::EMPTY;
    actor_instance->mailbox.emplace_back(MailboxItem { instance_id, message, behaviour_fn });
    if(actor_instance->state.compare_exchange_strong(expected, State::RUNNABLE)) {
        // We are doing compare_exchange_strong so that only one 
        runtime_ds->schedule_queue.emplace_back(instance_id);
        runtime_ds->thread_bed.release();
    }
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

// Returns true if lock acquired, false if caller must suspend.
// CR: Note that this is a very thin wrapper. Think carefully about whether
// this is needed
bool handle_lock(uint64_t actor_instance_id, uint64_t lock_id) {
    assert(runtime_ds->mutex_map.find(lock_id) != runtime_ds->mutex_map.end());
    UserMutex& mtx = runtime_ds->mutex_map[lock_id];
    return mtx.lock(actor_instance_id);
}