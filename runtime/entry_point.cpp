#include "runtime_traps.hpp"
#include <iostream>
#include <assert.h>

extern "C" void coherence_initialize();
extern "C" uint64_t num_locks;

void runtime_initialize() {
    runtime_ds = new RuntimeDS();
    runtime_ds->instances_created = 0;
    for(uint64_t lock_id = 0; lock_id < num_locks; lock_id++) {
        runtime_ds->mutex_map.emplace(lock_id);
    }
    coherence_initialize();
}

void runtime_activate_actor(std::uint64_t instance_id) {
    runtime_ds->schedule_queue.emplace_back(instance_id);
}

void thread_loop() {
    using State = ActorInstanceState::State;

    while (true) {

        auto actor_instance_id_opt = runtime_ds->schedule_queue.try_pop_front();
        if (!actor_instance_id_opt) {
            // CR: Implement correct behaviour here
            continue;
        }

        uint64_t actor_instance_id = *actor_instance_id_opt;

        auto actor_instance_state_opt = 
            runtime_ds->id_actor_instance_map.get_value(actor_instance_id);
        assert(actor_instance_state_opt != std::nullopt);
        auto actor_instance_state = *actor_instance_state_opt;

        State expected = State::RUNNABLE;
        assert(actor_instance_state->state.compare_exchange_strong(expected, State::RUNNING));
        auto msg_opt = actor_instance_state->mailbox.try_pop_front();
        assert(msg_opt != std::nullopt);
        MailboxItem msg = *msg_opt;
        
        bool waiting_on_lock = false;
        while (true) {
            SuspendTag tag = msg.resume_fn(msg.frame);
            switch(tag.kind) {
                case SuspendTagKind::RETURN:
                    // Behaviour finished
                    break;
                case SuspendTagKind::LOCK:
                    bool acquired_lock = handle_lock(actor_instance_id, tag.lock_id);
                    if(acquired_lock) {
                        continue;
                    }
                    waiting_on_lock = true;
                    actor_instance_state->state.store(State::WAITING);
                    break;
                default:
                    assert(false);
            }
        }

        if(!waiting_on_lock) {
            actor_instance_state->state.store(State::EMPTY);
            State expected = State::EMPTY;
            if (!actor_instance_state->mailbox.empty()) {
                actor_instance_state->state.store(State::RUNNABLE);
                runtime_ds->schedule_queue.emplace_back(actor_instance_id);
                return;
            }
        }
    }
}


int main() {
    runtime_initialize();
    coherence_initialize();
    thread_loop();

    return 0;
}
