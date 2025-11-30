#include "runtime_traps.hpp"
#include <iostream>
#include <assert.h>
#include <vector>
#include <thread>
#include <condition_variable>


extern "C" void coherence_initialize();
extern "C" uint64_t num_locks;

void runtime_initialize() {
    runtime_ds = new RuntimeDS();
    runtime_ds->instances_created = 0;
    runtime_ds->threads_asleep = 0;
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
            // Check whether the sleep counter is 31. That means the program is ending.
            runtime_ds->threads_asleep++;
            if(runtime_ds->threads_asleep == 32) {
                for (int i = 0; i < 32; ++i) {
                    runtime_ds->thread_bed.release();
                }
                std::terminate();
            }
            else {
                runtime_ds->thread_bed.acquire();
                if(runtime_ds->threads_asleep == 32) {
                    std::terminate();
                }
                runtime_ds->threads_asleep--;
                continue;
            }
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
                    msg.destroy_fn(msg.frame);
                    break;
                case SuspendTagKind::LOCK:
                    bool acquired_lock = handle_lock(actor_instance_id, tag.lock_id);
                    if(acquired_lock) {
                        continue;
                    }
                    waiting_on_lock = true;
                    actor_instance_state->state = State::WAITING;
                    actor_instance_state->mailbox.emplace_front(msg);
                    break;
                default:
                    assert(false);
            }
        }

        if(!waiting_on_lock) {
            actor_instance_state->state = State::EMPTY;
            State expected = State::EMPTY;
            if (!actor_instance_state->mailbox.empty()) {
                actor_instance_state->state = State::RUNNABLE;
                runtime_ds->schedule_queue.emplace_back(actor_instance_id);
                return;
            }
        }
    }
}


int main() {
    runtime_initialize();

    const int NUM_THREADS = 32;
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(&thread_loop);
    }

    for (auto &t : workers) {
        t.join();
    }

    return 0;
}
