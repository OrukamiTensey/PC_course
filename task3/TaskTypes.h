#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <any>
#include <optional>

// Перелік можливих статусів задачі відповідно до вимог варіанту
enum class TaskStatus {
    Pending,     // Задача чекає в черзі
    Running,     // Задача виконується воркером
    Completed,   // Задача успішно виконана
    Rejected     // Задача відкинута через переповнення черги
};

// Структура для збереження метаданих та результату задачі
struct TaskResult {
    TaskStatus status = TaskStatus::Pending;
    std::any value;                             // Зберігає будь-який тип результату (C++17)
    std::string errorMessage;                    // Якщо під час виконання виник виняток
};

// Потокобезпечний менеджер для відстеження статусів та результатів за ID
class TaskResultManager {
public:
    TaskResultManager() = default;

    // Реєстрація нової задачі
    void register_task(size_t id) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_results[id] = { TaskStatus::Pending, std::any(), "" };
    }

    // Оновлення статусу задачі
    void update_status(size_t id, TaskStatus status) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (m_results.find(id) != m_results.end()) {
            m_results[id].status = status;
        }
    }

    // Запис успішного результату
    void set_result(size_t id, std::any result) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (m_results.find(id) != m_results.end()) {
            m_results[id].status = TaskStatus::Completed;
            m_results[id].value = std::move(result);
        }
    }

    // Запис помилки, якщо задача викинула виняток
    void set_error(size_t id, const std::string& error) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (m_results.find(id) != m_results.end()) {
            m_results[id].status = TaskStatus::Rejected;
            m_results[id].errorMessage = error;
        }
    }

    // Отримання поточного статусу задачі
    TaskStatus get_status(size_t id) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex); 
        auto it = m_results.find(id);
        if (it != m_results.end()) {
            return it->second.status;
        }
        return TaskStatus::Rejected;
    }

    // Отримання копії структури результату
    std::optional<TaskResult> get_result(size_t id) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_results.find(id);
        if (it != m_results.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    mutable std::shared_mutex m_mutex; // Використовуємо shared_mutex для оптимізації читання
    std::unordered_map<size_t, TaskResult> m_results;
};