#pragma once
#include <queue>
#include <functional>
#include <mutex>
#include <chrono>
#include <vector>
#include <algorithm>

class BoundedTaskQueue {
public:
    using TaskType = std::pair<size_t, std::function<void()>>; // Співвідношення: {Task_ID, Функція}

    explicit BoundedTaskQueue(size_t max_size = 20)
        : m_max_size(max_size), m_is_full(false) {}

    // Спроба додати задачу. Повертає true, якщо додано успішно, і false, якщо відкинуто
    bool try_push(size_t id, std::function<void()> task) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_tasks.size() >= m_max_size) {
            return false; // Черга переповнена, задача відкидається відповідно до варіанту 17
        }

        m_tasks.push({ id, std::move(task) });

        // Якщо черга щоправда щойно заповнилася до максимуму, фіксуємо час
        if (m_tasks.size() == m_max_size && !m_is_full) {
            m_is_full = true;
            m_full_start_time = std::chrono::steady_clock::now();
        }

        return true;
    }

    // Отримання задачі з черги (для воркерів)
    bool pop(TaskType& out_task) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_tasks.empty()) {
            return false;
        }

        out_task = std::move(m_tasks.front());
        m_tasks.pop();

        // Якщо черга була заповнена, а тепер звільнилося хоча б одне місце
        if (m_is_full && m_tasks.size() < m_max_size) {
            m_is_full = false;
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_full_start_time).count();
            m_full_durations.push_back(duration); // Зберігаємо час у мілісекундах, поки черга була заповнена
        }

        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

    // Метод для очищення черги при екстреній зупинці пулу
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_tasks.empty()) {
            m_tasks.pop();
        }
        if (m_is_full) {
            m_is_full = false;
        }
    }

    // Отримання зібраних метрик заповненості черги (для пункту 6 методички)
    void get_fullness_metrics(long long& min_time, long long& max_time) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_full_durations.empty()) {
            min_time = 0;
            max_time = 0;
            return;
        }
        min_time = *std::min_element(m_full_durations.begin(), m_full_durations.end());
        max_time = *std::max_element(m_full_durations.begin(), m_full_durations.end());
    }

private:
    const size_t m_max_size;
    std::queue<TaskType> m_tasks;
    mutable std::mutex m_mutex;

    // Метрики заповненості черги
    bool m_is_full;
    std::chrono::steady_clock::time_point m_full_start_time;
    std::vector<long long> m_full_durations; // Зберігає проміжки часу заповненості в мс
};