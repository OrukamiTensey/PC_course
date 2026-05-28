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

    void print_metrics() const;
    TaskStatus get_task_status(size_t id) const { return m_result_manager.get_status(id); }

private:
    // Головний робочий цикл кожного з 6 потоків-воркерів
    void routine(size_t worker_id) {
        while (true) {
            BoundedTaskQueue::TaskType current_task;
            bool task_acquired = false;

            {
                std::unique_lock<std::mutex> lock(m_pool_mutex);

                // Умова очікування: пул на паузі АБО (черга порожня і пул продовжує роботу)
                m_task_waiter.wait(lock, [this, &task_acquired, &current_task]() {
                    // Якщо пул терміновано, і ми або зупиняємось негайно, або черга вже порожня
                    if (m_terminated && (m_immediate_shutdown || m_tasks.empty())) {
                        return true;
                    }
                    // Якщо пул не на паузі, пробуємо взяти задачу
                    if (!m_paused) {
                        task_acquired = m_tasks.pop(current_task);
                    }
                    // Прокидаємо потік, якщо є задача, або якщо пул зупиняють
                    return m_terminated || task_acquired;
                    });

                // Перевірка умов виходу з потоку
                if (m_terminated && !task_acquired) {
                    if (m_immediate_shutdown || m_tasks.empty()) {
                        return; // Потік завершує свою роботу
                    }
                }
            }

            // Якщо задачу успішно взято з черги — виконуємо її поза м'ютексом пулу
            if (task_acquired) {
                size_t task_id = current_task.first;
                auto& task_function = current_task.second;

                m_result_manager.update_status(task_id, TaskStatus::Running);

                try {
                    task_function(); // Виконання безпосередньо самої задачі
                    m_result_manager.update_status(task_id, TaskStatus::Completed);
                }
                catch (const std::exception& e) {
                    m_result_manager.set_error(task_id, e.what());
                }
                catch (...) {
                    m_result_manager.set_error(task_id, "Unknown exception occurred");
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
