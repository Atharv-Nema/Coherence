#pragma once
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <deque>
#include <mutex>
#include <optional>
#include <atomic>
#include <assert.h>
#include <semaphore>
#include <boost/context/detail/fcontext.hpp>

namespace boost_ctx = boost::context::detail;

struct RuntimeDS;

struct MailboxItem {
    uint64_t actor_id;
    void* message;
    void (*behaviour_fn)(void*);
};

template <typename T>
class ThreadSafeDeque {
private:
    std::mutex dq_lock;
    std::deque<T> dq;

public:
    void emplace_back(const T& item) {
        std::lock_guard<std::mutex> lock(dq_lock);
        dq.emplace_back(item);
    }

    void emplace_front(const T& item) {
        std::lock_guard<std::mutex> lock(dq_lock);
        dq.emplace_front(item);
    }

    std::optional<T> try_pop_front() {
        std::lock_guard<std::mutex> lock(dq_lock);
        if (dq.empty()) return std::nullopt;
        T front_ele = dq.front();
        dq.pop_front();
        return front_ele;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(dq_lock);
        return dq.empty();
    }

    std::size_t size() {
        std::lock_guard<std::mutex> lock(dq_lock);
        return dq.size();
    }
};

template <typename K, typename V>
class ThreadSafeMap {
private:
    std::mutex map_lock;
    std::unordered_map<K, V> map;

public:
    void insert(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(map_lock);
        map.emplace(key, value);
    }

    std::optional<V> get_value(const K& key) {
        std::lock_guard<std::mutex> lock(map_lock);
        auto it = map.find(key);
        if (it == map.end()) {
            return std::nullopt;
        }
        return it->second;
    }
};

// User mutex is a reentrant lock at the source level
class UserMutex {
private:
    // A lock for exclusive access to [UserMutex] members
    std::mutex coord_lock;
    std::optional<uint64_t> holding_instance;
    // An actor instance can call [lock] multiple times. This simply increments
    // [num_lock_called], which is decremented when [unlock] is called. The lock
    // is released when [num_lock_called] reaches 0.
    uint64_t num_lock_called = 0;
    // Queue of ids of actor instances waiting on this mutex
    std::deque<uint64_t> wait_queue;

public:
    bool lock(RuntimeDS* runtime, uint64_t instance_id);
    void unlock(RuntimeDS* runtime);
};

// [mailbox] and [state] are the fields that need protection
struct ActorInstanceState {
    enum class State {EMPTY, WAITING, RUNNABLE, RUNNING};
    State state;
    std::mutex instance_lock;
    void* llvm_actor_object;
    // CR: Think carefully about the initialization status to avoid UB
    boost_ctx::fcontext_t next_continuation;
    void* running_be_sp;
    const uint64_t instance_id;
    std::deque<MailboxItem> mailbox;
    ActorInstanceState(void* llvm_actor_object, const uint64_t instance_id)
        : instance_id(instance_id) {
        state = ActorInstanceState::State::EMPTY;
        this->llvm_actor_object = llvm_actor_object;
        next_continuation = nullptr;
        running_be_sp = nullptr;
    }

};

struct RuntimeDS {
    // When the schedule queue is empty, threads can sleep in [thread_bed]
    std::binary_semaphore thread_bed;
    std::atomic<uint8_t> threads_asleep;
    std::atomic<uint64_t> instances_created;
    ThreadSafeDeque<uint64_t> schedule_queue;
    ThreadSafeMap<uint64_t, std::shared_ptr<ActorInstanceState>> id_actor_instance_map;
    std::unordered_map<uint64_t, UserMutex> mutex_map;
    RuntimeDS(): thread_bed(0) {}
};

inline bool UserMutex::lock(RuntimeDS* runtime_ds, uint64_t instance_id) {
    // Atomic section for mutual exclusion
    std::lock_guard<std::mutex> lock_guard(coord_lock);
    if(holding_instance == std::nullopt) {
        holding_instance = instance_id;
    }
    if(holding_instance == instance_id) {
        num_lock_called++;
        return true;
    }
    auto actor_instance_state_opt = runtime_ds->id_actor_instance_map.get_value(instance_id);
    assert(actor_instance_state_opt != std::nullopt);
    std::shared_ptr<ActorInstanceState> actor_instance_state = actor_instance_state_opt.value();
    std::lock_guard<std::mutex> instance_guard(actor_instance_state->instance_lock);
    actor_instance_state->state = ActorInstanceState::State::WAITING;
    MailboxItem dummy_msg{instance_id, nullptr, nullptr};
    actor_instance_state->mailbox.emplace_front(dummy_msg);
    wait_queue.emplace_back(instance_id);
    return false;
}

inline void UserMutex::unlock(RuntimeDS* runtime) {
    using State = ActorInstanceState::State;
    // Atomic section for mutual exclusion
    std::lock_guard<std::mutex> lock_guard(coord_lock);
    num_lock_called--;
    if(num_lock_called > 0) {
        return;
    }

    // Try to wake the next waiting actor
    if(wait_queue.size() == 0) {
        // Unlock the mutex
        holding_instance = std::nullopt;
        return;
    }
    uint64_t actor_instance_id = wait_queue.front();
    wait_queue.pop_front();

    // Lookup actor
    auto actor_instance_state_opt = 
        runtime->id_actor_instance_map.get_value(actor_instance_id);
    assert(actor_instance_state_opt != std::nullopt);
    std::shared_ptr<ActorInstanceState> actor_instance_state = *actor_instance_state_opt;
    // Now acquire the instance lock
    std::lock_guard<std::mutex> instance_guard(actor_instance_state->instance_lock);
    assert(!actor_instance_state->mailbox.empty());
    assert(actor_instance_state->state == State::WAITING);
    actor_instance_state->state = State::RUNNABLE;
    // Giving the lock to [actor_instance_id]
    holding_instance = actor_instance_id;
    num_lock_called = 1;
    runtime->schedule_queue.emplace_back(actor_instance_id);
    runtime->thread_bed.release();
    return;
}
