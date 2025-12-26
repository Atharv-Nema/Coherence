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

class UserMutex {
private:
    std::atomic<bool> locked = false;
    // Queue of ids of actor instances waiting on this mutex
    ThreadSafeDeque<std::uint64_t> wait_queue;

public:
    bool lock(std::uint64_t instance_id);
    void unlock(RuntimeDS* runtime);
};

struct ActorInstanceState {
    enum class State {EMPTY, WAITING, RUNNABLE, RUNNING};
    std::atomic<State> state;
    void* llvm_actor_object;
    // CR: Think carefully about the initialization status to avoid UB
    boost_ctx::fcontext_t next_continuation;
    void* running_be_sp;
    const std::uint64_t instance_id;
    ThreadSafeDeque<MailboxItem> mailbox;
    ActorInstanceState(void* llvm_actor_object, const std::uint64_t instance_id)
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
    std::atomic<std::uint8_t> threads_asleep;
    std::atomic<std::uint64_t> instances_created;
    ThreadSafeDeque<std::uint64_t> schedule_queue;
    ThreadSafeMap<std::uint64_t, std::shared_ptr<ActorInstanceState>> id_actor_instance_map;
    std::unordered_map<std::uint64_t, UserMutex> mutex_map;
    RuntimeDS(): thread_bed(0) {}
};

inline bool UserMutex::lock(std::uint64_t instance_id) {
    // Returns true if acquired immediately, false if suspended.
    bool expected = false;
    // CR: In later stages, may want to experiment with more efficient memory models
    if (locked.compare_exchange_strong(expected, true, std::memory_order_seq_cst)) {
        return true;
    }
    wait_queue.emplace_back(instance_id);
    return false;
}

inline void UserMutex::unlock(RuntimeDS* runtime) {
    using State = ActorInstanceState::State;

    // Try to wake the next waiting actor
    std::optional<std::uint64_t> actor_instance_id_opt = wait_queue.try_pop_front();

    if (actor_instance_id_opt) {
        std::uint64_t actor_instance_id = *actor_instance_id_opt;

        // Lookup actor
        auto actor_instance_state_opt = 
            runtime->id_actor_instance_map.get_value(actor_instance_id);
        assert(actor_instance_state_opt != std::nullopt);
        auto actor_instance_state = *actor_instance_state_opt;
        assert(!actor_instance_state->mailbox.empty());
        State expected = State::WAITING;
        bool successful_exchange = actor_instance_state->state.compare_exchange_strong(
            expected, State::RUNNABLE
        );
        // Actor instance can only be waiting on one lock at any given point of time. Race
        // conditions should not occur.
        assert(successful_exchange);
        runtime->schedule_queue.emplace_back(actor_instance_id);
        return;
    }
    locked = false;
}
