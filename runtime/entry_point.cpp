#include "runtime_traps.hpp"
#include <iostream>
#include <assert.h>
#include <vector>
#include <thread>
#include <condition_variable>

// CR: May not need [coherence_initialize]. Probably better to directly create [#Start] actor instance 
// and call its behaviour
extern "C" void coherence_initialize();
extern "C" uint64_t num_locks;

// 256KB stacks
static std::size_t stack_size = 256 * 1024;
RuntimeDS* runtime_ds;

void runtime_initialize() {
    runtime_ds = new RuntimeDS();
    runtime_ds->instances_created = 0;
    runtime_ds->threads_asleep = 0;
    for(uint64_t lock_id = 0; lock_id < num_locks; lock_id++) {
        runtime_ds->mutex_map.try_emplace(lock_id);
    }
    coherence_initialize();
}

void call_behaviour_context(boost_ctx::transfer_t t) {
    boost_ctx::fcontext_t main_ctx = t.fctx;
    MailboxItem* mailbox_item = reinterpret_cast<MailboxItem*>(t.data);
    auto actor_instance_opt = runtime_ds->id_actor_instance_map.get_value(mailbox_item->actor_id); 
    assert(actor_instance_opt != std::nullopt);
    auto actor_instance = *actor_instance_opt;
    actor_instance->next_continuation = main_ctx;
    mailbox_item->behaviour_fn(mailbox_item->message);
    // Should never reach here
    assert(false);
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
                // std::cout << "num actor instances" << runtime_ds->instances_created << std::endl;
                return;
            }
            else {
                runtime_ds->thread_bed.acquire();
                if(runtime_ds->threads_asleep == 32) {
                    return;
                }
                runtime_ds->threads_asleep--;
                continue;
            }
            assert(false); // No control path reaches here
        }
        uint64_t actor_instance_id = *actor_instance_id_opt;

        auto actor_instance_state_opt = 
            runtime_ds->id_actor_instance_map.get_value(actor_instance_id);
        assert(actor_instance_state_opt != std::nullopt);
        std::shared_ptr<ActorInstanceState> actor_instance_state = *actor_instance_state_opt;

        State expected = State::RUNNABLE;
        bool set_running = actor_instance_state->state.compare_exchange_strong(expected, State::RUNNING);
        assert(set_running);
        auto msg_opt = actor_instance_state->mailbox.try_pop_front();
        assert(msg_opt != std::nullopt);
        MailboxItem msg = *msg_opt;
        // If the actor_instace_state->next_continuation != std::nullptr, this means that we need to
        // call that continuation
        if(actor_instance_state->next_continuation == nullptr) {
            // Need to pop message from the stack and fill out the continuation
            assert(actor_instance_state->running_be_sp == nullptr);
            void *sp = std::malloc(stack_size);
            actor_instance_state->next_continuation = boost_ctx::make_fcontext(
                static_cast<char*>(sp) + stack_size, stack_size, call_behaviour_context);
            actor_instance_state->running_be_sp = sp;
        }
        
        bool loop_done = false;
        while (!loop_done) {
            boost_ctx::transfer_t t = boost_ctx::jump_fcontext(
                actor_instance_state->next_continuation, &msg);
            SuspendTag* tag = reinterpret_cast<SuspendTag*>(t.data); 
            switch(tag->kind) {
                case SuspendTagKind::RETURN:
                    actor_instance_state->next_continuation = nullptr;
                    std::free(actor_instance_state->running_be_sp);
                    actor_instance_state->running_be_sp = nullptr;
                    loop_done = true;
                    actor_instance_state->state = State::EMPTY;
                    if (!actor_instance_state->mailbox.empty()) {
                        State expected = State::EMPTY;
                        if (actor_instance_state->state.compare_exchange_strong(expected, State::RUNNABLE)) {
                            runtime_ds->schedule_queue.emplace_back(actor_instance_id);
                            runtime_ds->thread_bed.release();
                        }
                    }
                    break;
                case SuspendTagKind::LOCK: {
                    // Need to make sure that when [actor_instance_state] is added, it has the
                    // correct continuation
                    actor_instance_state->next_continuation = t.fctx;
                    assert(runtime_ds->mutex_map.find(tag->lock_id) != runtime_ds->mutex_map.end());
                    UserMutex& mtx = runtime_ds->mutex_map[tag->lock_id];
                    bool acquired_lock = mtx.lock(runtime_ds, actor_instance_id);
                    if(acquired_lock) {
                        continue;
                    }
                    loop_done = true;
                    break;
                }
                default:
                    assert(false);
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
