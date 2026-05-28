#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "TaskTypes.h"
#include "BoundedTaskQueue.h"

class ThreadPool {
public:
    // Конструктор ініціалізує менеджер та чергу
    ThreadPool()
        : m_tasks(20) // Черга обмежена 20 задачами відповідно до варіанту 17
        , m_initialized(false)
        , m_terminated(false)
        , m_paused(false)
        , m_task_id_counter(0)
    {}

    // Деструктор автоматично викликає термінацію
    ~ThreadPool() {
        terminate(false); // За замовчуванням плавно доробляємо задачі
    }

    // Ініціалізація пулу та створення 6 робочих потоків
    void initialize() {
        std::unique_lock<std::mutex> lock(m_pool_mutex);
        if (m_initialized || m_terminated) return;

        const size_t worker_count = 6; // Специфікація Варіанту 17: 6 робочих потоків
        m_workers.reserve(worker_count);

        for (size_t id = 0; id < worker_count; ++id) {
            m_workers.emplace_back(&ThreadPool::routine, this, id);
        }

        m_initialized = true;
    }

    // Тимчасова зупинка роботи пулу (пауза)
    void pause() {
        m_paused = true;
    }

    // Відновлення роботи пулу після паузи
    void resume() {
        m_paused = false;
        m_task_waiter.notify_all(); // Прокидаємо всі потоки для продовження роботи
    }

    // Прототипи інших публічних методів, які ми деталізуємо в наступних комітах
    // Реалізація безпечного додавання задач (Варіант 17)
    template <typename TaskF, typename... Args>
    size_t add_task(TaskF&& task, Args&&... parameters) {
        // 1. Генеруємо унікальний ID для нової задачі
        size_t task_id = ++m_task_id_counter;

        {
            std::unique_lock<std::mutex> lock(m_pool_mutex);
            // Якщо пул закривається або закритий, не приймаємо задачі
            if (m_terminated) {
                m_result_manager.register_task(task_id);
                m_result_manager.update_status(task_id, TaskStatus::Rejected);
                m_rejected_tasks_count++; // Збільшуємо лічильник відкинутих задач для метрик
                return task_id;
            }
        }

        // 2. Реєструємо задачу в менеджері результатів
        m_result_manager.register_task(task_id);

        // 3. Зв'язуємо функцію з її аргументами за допомогою std::bind або лямбди
        auto bound_task = std::bind(std::forward<TaskF>(task), std::forward<Args>(parameters)...);

        // 4. Спроба проштовхнути задачу в обмежену чергу
        bool pushed = m_tasks.try_push(task_id, bound_task);

        if (!pushed) {
            // Якщо черга заповнена (макс 20), задача відкидається відповідно до варіанту
            m_result_manager.update_status(task_id, TaskStatus::Rejected);
            m_rejected_tasks_count++; // Фіксуємо відкинуту задачу для майбутніх метрик пункту 6
        }
        else {
            // Якщо задача успішно додана, сповіщаємо один вільний потік
            m_task_waiter.notify_one();
        }

        return task_id; // Повертаємо ID користувачу
    }

    // Реалізація завершення роботи пулу потоків (плавне або моментальне)
    void terminate(bool immediate = false) {
        {
            std::unique_lock<std::mutex> lock(m_pool_mutex);

            // Якщо пул вже зупинено або в процесі зупинки — нічого не робимо
            if (m_terminated) return;

            m_terminated = true;
            m_immediate_shutdown = immediate;

            // Якщо режим моментальний, повністю очищаємо чергу невиконаних задач
            if (m_immediate_shutdown) {
                m_tasks.clear();
            }
        }

        // Прокидаємо абсолютно всі потоки, які могли застрягти на умовній змінній чи паузі
        m_task_waiter.notify_all();

        // Очікуємо на завершення виконання кожного з 6 потоків-робітників
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        // Очищаємо вектор потоків після їх успішного завершення
        std::unique_lock<std::mutex> lock(m_pool_mutex);
        m_workers.clear();
        m_initialized = false;
    }

    // Розрахунок та виведення метрик у консоль відповідно до Пункту 6 методички
    void print_metrics() const {
        std::unique_lock<std::mutex> lock(m_pool_mutex);

        std::cout << "\n=============================================\n";
        std::cout << "   СТАТИСТИКА ТА МЕТРИКИ (ВАРІАНТ №17)\n";
        std::cout << "=============================================\n";

        // 1. Кількість створених потоків
        std::cout << "* Кількість створених робочих потоків: 6\n"; 

            // 2. Середній час знаходження кожного потоку в стані очікування
            std::cout << "* Середній час очікування потоків (воркерів):\n"; 
            double total_avg = 0.0;
        for (size_t i = 0; i < 6; ++i) {
            double avg_wait = 0.0;
            if (i < m_worker_wait_counts.size() && m_worker_wait_counts[i] > 0) {
                avg_wait = static_cast<double>(m_worker_wait_durations[i]) / m_worker_wait_counts[i];
            }
            std::cout << "  - Потік #" << i << ": " << avg_wait << " мс (запитів очікування: " << m_worker_wait_counts[i] << ")\n";
            total_avg += avg_wait;
        }
        std::cout << "  -> Загальний середній час очікування пулу: " << (total_avg / 6.0) << " мс\n";

        // 3. Мінімальний та максимальний час заповненості черги
        long long min_full_time = 0;
        long long max_full_time = 0;
        m_tasks.get_fullness_metrics(min_full_time, max_full_time); 
            std::cout << "* Час, поки черга була повністю заповнена (ліміт 20 задач):\n"; 
            std::cout << "  - Мінімальний час: " << min_full_time << " мс\n"; 
            std::cout << "  - Максимальний час: " << max_full_time << " мс\n"; 

            // 4. Кількість відкинутих задач
            std::cout << "* Кількість відкинутих задач (через переповнення): " << m_rejected_tasks_count << "\n"; 
            std::cout << "=============================================\n\n";
    }
    TaskStatus get_task_status(size_t id) const { return m_result_manager.get_status(id); }

private:
    std::vector<long long> m_worker_wait_durations; // Загальний час очікування для кожного воркера (в мс)
    std::vector<size_t> m_worker_wait_counts;       // Кількість разів, коли воркер ставав на очікування
    // Головний робочий цикл кожного з 6 потоків-воркерів
    void routine(size_t worker_id) {
        // Ініціалізуємо вектори метрик для поточного воркера під м'ютексом
        {
            std::unique_lock<std::mutex> lock(m_pool_mutex);
            if (m_worker_wait_durations.size() <= worker_id) {
                m_worker_wait_durations.resize(6, 0);
                m_worker_wait_counts.resize(6, 0);
            }
        }

        while (true) {
            BoundedTaskQueue::TaskType current_task;
            bool task_acquired = false;

            {
                std::unique_lock<std::mutex> lock(m_pool_mutex);

                // Фіксуємо час початку очікування
                auto wait_start = std::chrono::steady_clock::now();
                m_worker_wait_counts[worker_id]++;

                m_task_waiter.wait(lock, [this, &task_acquired, &current_task]() {
                    if (m_terminated && (m_immediate_shutdown || m_tasks.empty())) {
                        return true;
                    }
                    if (!m_paused) {
                        task_acquired = m_tasks.pop(current_task);
                    }
                    return m_terminated || task_acquired;
                    });

                // Фіксуємо час закінчення очікування і додаємо до загальної суми воркера
                auto wait_end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(wait_end - wait_start).count();
                m_worker_wait_durations[worker_id] += duration;

                if (m_terminated && !task_acquired) {
                    if (m_immediate_shutdown || m_tasks.empty()) {
                        return;
                    }
                }
            }

            if (task_acquired) {
                size_t task_id = current_task.first;
                auto& task_function = current_task.second;

                m_result_manager.update_status(task_id, TaskStatus::Running);

                try {
                    task_function();
                    m_result_manager.update_status(task_id, TaskStatus::Completed);
                }
                catch (...) {
                    m_result_manager.set_error(task_id, "Exception in task execution");
                }
            }
        }
    }

private:
    BoundedTaskQueue m_tasks;
    TaskResultManager m_result_manager;
    std::vector<std::thread> m_workers;

    mutable std::mutex m_pool_mutex;
    std::condition_variable m_task_waiter;

    std::atomic<bool> m_initialized;
    std::atomic<bool> m_terminated;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_immediate_shutdown{ false };

    std::atomic<size_t> m_task_id_counter;
    std::atomic<size_t> m_rejected_tasks_count{ 0 }; 
};
