#include "runtime_traps.hpp"
#include <iostream>
#include <assert.h>
#include <vector>
#include <thread>
#include <condition_variable>

/*
Lock order:
(lock mutex) ---> (Actor-instance lock) --> (schedule queue lock)
*/


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
        uint64_t actor_instance_id;
        {
            std::unique_lock<std::mutex> lock_guard(runtime_ds->schedule_queue_lock);
            while(runtime_ds->schedule_queue.size() == 0) {
                runtime_ds->threads_asleep++;
                if(runtime_ds->threads_asleep == 16) {
                    runtime_ds->thread_bed.notify_all();
                    return;
                }
                else {
                    runtime_ds->thread_bed.wait(lock_guard);
                    if(runtime_ds->threads_asleep == 16) {
                        return;
                    }
                    runtime_ds->threads_asleep--;
                }
            }
            actor_instance_id = runtime_ds->schedule_queue.front();
            runtime_ds->schedule_queue.pop_front();
        }

        auto actor_instance_state_opt = 
            runtime_ds->id_actor_instance_map.get_value(actor_instance_id);
        assert(actor_instance_state_opt != std::nullopt);
        std::shared_ptr<ActorInstanceState> actor_instance_state = *actor_instance_state_opt;

        MailboxItem msg;
        {
            std::lock_guard<std::mutex> instance_guard(actor_instance_state->instance_lock);
            assert(actor_instance_state->state == State::RUNNABLE);
            actor_instance_state->state = State::RUNNING;
            assert(!actor_instance_state->mailbox.empty());
            msg = actor_instance_state->mailbox.front();
            actor_instance_state->mailbox.pop_front();
        }
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
                    {
                        std::lock_guard<std::mutex> instance_guard(actor_instance_state->instance_lock);
                        if(actor_instance_state->mailbox.empty()) {
                            actor_instance_state->state = State::EMPTY;
                        }
                        else {
                            actor_instance_state->state = State::RUNNABLE;
                            {
                                std::lock_guard<std::mutex> thread_guard(runtime_ds->schedule_queue_lock);
                                runtime_ds->schedule_queue.emplace_back(actor_instance_id);
                                runtime_ds->thread_bed.notify_one();
                            }
                            
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

    const int NUM_THREADS = 16;
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
