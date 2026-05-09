#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <condition_variable>
#include <cassert>

class Actor;

using BehvPtr = void (Actor::*)(void*);
struct MailboxItem {
    BehvPtr behv;
    void* arg;
};

struct RuntimeDS {
    std::deque<Actor*> schedule_queue;
    std::mutex queue_mtx;
    std::atomic<int> running_behvs = 0;
    std::condition_variable cv;
};

RuntimeDS* runtime_ds;

enum State {RUNNING, NOT_RUNNING};
class Actor {
private:
    int id;
    std::vector<Actor*>* peers;
    int num_peers;
    int pings_remaining;
    std::deque<MailboxItem> mailbox;
    std::mutex actor_mtx;
    State state = NOT_RUNNING;
    

public:
    // Runtime trap
    void run_one_behv() {
        runtime_ds->running_behvs++;
        std::lock_guard<std::mutex> lock(actor_mtx);
        assert(mailbox.size() > 0);
        assert(state == NOT_RUNNING);
        state = RUNNING;
        MailboxItem item = mailbox.front();
        mailbox.pop_front();
        std::thread([this, item]() {
            (this->*(item.behv))(item.arg);
        }).detach();
    }
    void handle_behv_call(MailboxItem item) {
        // Locking convention: First queue_lock, then actor_lock
        std::lock_guard<std::mutex> queue_lock(runtime_ds->queue_mtx);
        std::lock_guard<std::mutex> actor_lock(actor_mtx);
        mailbox.push_back(item);
        if(state != RUNNING && mailbox.size() == 1) {
            runtime_ds->schedule_queue.push_back(this);
            runtime_ds->cv.notify_one();
        }
    }
    
    void handle_return() {
        std::lock_guard<std::mutex> queue_lock(runtime_ds->queue_mtx);
        std::lock_guard<std::mutex> actor_lock(actor_mtx);
        state = NOT_RUNNING;
        if(mailbox.size() > 0) {
            runtime_ds->schedule_queue.push_back(this);
        }
        runtime_ds->running_behvs--;
        runtime_ds->cv.notify_one();
    }
    
    struct Defer {
        Actor* actor;
        Defer(Actor* actor){
            this->actor = actor;
        }
        ~Defer(){
            actor->handle_return();
        }
    };

    // Constructor
    Actor(int id_arg, std::vector<Actor*>* peer_list, int n, int m)
        : id(id_arg), peers(peer_list), num_peers(n), pings_remaining(m) {}

    // Functions
    int random_gen() {
        int mixed = ((id + 1) * 1000003) + (pings_remaining * 500009);
        return mixed % num_peers;
    }
    void send_next_ping() {
        if (pings_remaining > 0) {
            int target_idx = random_gen();
            Actor* target = (*peers)[target_idx];
            target->handle_behv_call(MailboxItem{&Actor::receive_ping, this});
            pings_remaining--;
        }
    }

    // Behaviour (void* for uniformity)
    void start(void*) {
        Defer d(this);
        send_next_ping();
    }
    void receive_ping(void* arg) {
        Defer d(this);
        Actor* from = reinterpret_cast<Actor*>(arg);
        from->handle_behv_call(MailboxItem{&Actor::receive_pong, nullptr});
    }
    void receive_pong(void*) {
        // Spawning a thread for the receive_pong behavior
        Defer d(this);
        send_next_ping();
    }
};

void scheduler_loop() {
    while(true) {
        std::unique_lock<std::mutex> lock(runtime_ds->queue_mtx);
        runtime_ds->cv.wait(lock, []{ 
            return !runtime_ds->schedule_queue.empty() || runtime_ds->running_behvs == 0; 
        });
        if(runtime_ds->schedule_queue.empty() && runtime_ds->running_behvs == 0) {
            return;
        }
        assert(!runtime_ds->schedule_queue.empty());
        Actor* to_run = runtime_ds->schedule_queue.front();
        runtime_ds->schedule_queue.pop_front();
        assert(to_run != nullptr);
        to_run->run_one_behv();
    }
}

int main() {
    runtime_ds = new RuntimeDS;
    int n = 1000;
    int m = 100;
    std::vector<Actor*> peer_list;

    // 1. Initialize actors
    for (int i = 0; i < n; ++i) {
        peer_list.push_back(new Actor(i, &peer_list, n, m));
    }

    // 2. Add the start messages
    for (int i = 0; i < n; ++i) {
        peer_list[i]->handle_behv_call(MailboxItem{&Actor::start, nullptr});
    }

    // 3. Start the scheduler loop
    std::thread(&scheduler_loop).join();
    return 0;
}