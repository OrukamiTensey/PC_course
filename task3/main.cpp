#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <string_view>
#include <mutex>
#include "ThreadPool.h"

// Task function to simulate heavy work (Variant 17)
void simulated_heavy_task(size_t taskId) {
    // Setup random generator for 5 to 10 seconds execution time
    std::random_device rd;
    std::mt19937 device(rd());
    std::uniform_int_distribution<int> dist(5, 10);
    int sleep_duration = dist(device);

    // Simulate work by sleeping the thread
    std::this_thread::sleep_for(std::chrono::seconds(sleep_duration));
}

int main() {
    std::cout << "=== THREAD POOL TESTING SYSTEM INITIALIZED ===\n\n";

    // 1. Initialize Thread Pool (Will automatically create 6 workers)
    ThreadPool pool;
    pool.initialize();
    std::cout << "[POOL] Thread pool successfully initialized with 6 workers.\n";

    // Vector to keep track of generated task IDs
    std::vector<size_t> tracking_ids;
    std::mutex id_vector_mutex;

    // 2. Create 3 producer threads to add tasks concurrently
    const size_t producer_count = 3;
    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    std::cout << "[PRODUCERS] Launching " << producer_count << " producer threads for concurrent task injection...\n";

    for (size_t p_id = 0; p_id < producer_count; ++p_id) {
        producers.emplace_back([&pool, &tracking_ids, &id_vector_mutex]() {
            // Each producer attempts to push 12 tasks
            for (size_t j = 0; j < 12; ++j) {
                // Using modern lambda expression to avoid C++ bind deduction issues
                size_t task_id = pool.add_task([j]() {
                    simulated_heavy_task(j);
                    });

                {
                    std::lock_guard<std::mutex> lock(id_vector_mutex);
                    tracking_ids.push_back(task_id);
                }

                // High-speed injection ensures the queue (limit 20) fills up and rejects tasks
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            });
    }

    // Wait 2 seconds to let the queue fill up
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 3. Test PAUSE functionality
    std::cout << "\n[TEST] ---> PAUSING THREAD POOL FOR 3 SECONDS <---\n";
    pool.pause();

    // While paused, workers sleep, but producers keep injecting (triggering rejections)
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "[TEST] ---> RESUMING THREAD POOL WORK <---\n\n";
    pool.resume();

    // 4. Join all producer threads
    for (std::thread& producer : producers) {
        if (producer.joinable()) {
            producer.join();
        }
    }
    std::cout << "[PRODUCERS] All producer threads finished task generation.\n";

    // Allow the pool to process some tasks before making a status check
    std::this_thread::sleep_for(std::chrono::seconds(4));

    // 5. Query task statuses by their unique ID (Variant requirement)
    std::cout << "\n[STATUS CHECK] Querying current state of sample tasks by ID:\n";
    {
        std::lock_guard<std::mutex> lock(id_vector_mutex);
        size_t samples[] = { 1, 10, 30 };
        for (size_t sample_id : samples) {
            if (sample_id <= tracking_ids.size()) {
                TaskStatus status = pool.get_task_status(sample_id);
                std::cout << "  - Task ID #" << sample_id << " current status: ";
                switch (status) {
                case TaskStatus::Pending:   std::cout << "Pending (In Queue)\n"; break;
                case TaskStatus::Running:   std::cout << "Running (Executing)\n"; break;
                case TaskStatus::Completed: std::cout << "Completed (Success)\n"; break;
                case TaskStatus::Rejected:  std::cout << "Rejected (Queue Full)\n"; break;
                }
            }
        }
    }

    // 6. Graceful shutdown — wait for remaining tasks in the queue to finish
    std::cout << "\n[SHUTDOWN] Initiating graceful pool termination (waiting for remaining tasks)...\n";
    pool.terminate(false);
    std::cout << "[SHUTDOWN] Thread pool has been completely stopped.\n";

    // 7. Print final metric statistics collected during execution (Methodology Clause 6)
    pool.print_metrics();

    return 0;
}