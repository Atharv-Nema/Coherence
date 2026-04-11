#include <iostream>
#include <vector>
#include <thread>
#include <semaphore>
#include <chrono>


std::vector<std::binary_semaphore*> semaphores;

void worker_routine(int id, int* token) {
    int next_id = (id + 1) % semaphores.size();
    while (true) {
        semaphores[id]->acquire();
        if (*token > 0) {
            (*token)--;
            semaphores[next_id]->release();
        } else {
            semaphores[next_id]->release();
            return;
        }
    }
}

int main() {
    const int n_actors = 1000;
    const uint64_t m_passes = 1000000;

    int* token = new int(m_passes);
    std::vector<std::thread> threads;

    // Create semaphores
    for (int i = 0; i < n_actors; ++i) {
        semaphores.push_back(new std::binary_semaphore(0));
    }

    // Spawn threads
    for (int i = 0; i < n_actors; ++i) {
        threads.emplace_back(worker_routine, i, token);
    }

    // Toss the potato to the first worker
    semaphores[0]->release();

    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }
    for (auto s : semaphores) delete s;
    return 0;
}