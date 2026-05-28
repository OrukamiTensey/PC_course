#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include "ThreadPool.h"

// Функція, яку будуть виконувати робочі потоки пулу (імітація корисної роботи)
void simulated_heavy_task(size_t taskId) {
    // Налаштування генератора випадкових чисел для проміжку від 5 до 10 секунд (Варіант 17)
    std::random_device rd;
    std::mt199 device(rd());
    std::uniform_int_distribution<int> dist(5, 10);
    int sleep_duration = dist(device);

    // Імітуємо виконання за допомогою засинання потоку
    std::this_thread::sleep_for(std::chrono::seconds(sleep_duration));
}

int main() {
    // Встановлюємо кодування консолі для коректного виведення українською мовою
    std::string_view locale = "uk_UA.UTF-8";
    std::setlocale(LC_ALL, locale.data());

    std::cout << "=== ЗАПУСК СИСТЕМИ ТЕСТУВАННЯ ПУЛУ ПОТОКІВ ===\n\n";

    // 1. Створюємо та ініціалізуємо пул (він автоматично створить 6 воркерів)
    ThreadPool pool;
    pool.initialize();
    std::cout << "[POOL] Пул успішно ініціалізовано із 6-ма робочими потоками.\n";

    // Вектор для збереження ID доданих задач, щоб потім перевірити їхній статус
    std::vector<size_t> tracking_ids;
    std::mutex id_vector_mutex;

    // 2. Створюємо 3 окремі потоки-продюсери, які будуть паралельно закидати задачі в пул
    const size_t producer_count = 3;
    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    std::cout << "[PRODUCERS] Запуск 3 потоків-продюсерів для паралельного додавання задач...\n";

    for (size_t p_id = 0; p_id < producer_count; ++p_id) {
        producers.emplace_back([&pool, &tracking_ids, &id_vector_mutex, p_id]() {
            // Кожен продюсер намагається додати 12 задач з невеликим інтервалом
            for (size_t j = 0; j < 12; ++j) {
                size_t task_id = pool.add_task(simulated_heavy_task, j);

                {
                    std::lock_guard<std::mutex> lock(id_vector_mutex);
                    tracking_ids.push_back(task_id);
                }

                // Швидкість додавання вища, ніж швидкість обробки (6 воркерів по 5-10 сек),
                // що гарантовано призведе до повного заповнення черги (ліміт 20) та відкидання задач.
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            });
    }

    // Чекаємо 2 секунди, поки черга гарантовано наповниться задачми
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 3. Тестуємо механізм ПАУЗИ пулу
    std::cout << "\n[TEST] ---> АКТИВАЦІЯ ПАУЗИ ПУЛУ НА 3 СЕКУНДИ <---\n";
    pool.pause();

    // Поки пул на паузі, воркери не беруть задачі, але продюсери продовжують додавати (і заповнювати чергу)
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "[TEST] ---> ВІДНОВЛЕННЯ РОБОТИ ПУЛУ (RESUME) <---\n\n";
    pool.resume();

    // 4. Очікуємо завершення роботи всіх потоків-продюсерів
    for (std::thread& producer : producers) {
        if (producer.joinable()) {
            producer.join();
        }
    }
    std::cout << "[PRODUCERS] Усі потоки-продюсери завершили генерацію задач.\n";

    // Даємо пулу трохи часу попрацювати перед фіксацією статусів
    std::this_thread::sleep_for(std::chrono::seconds(4));

    // 5. Перевірка статусів декількох випадкових задач за їх ID (Вимога варіанту)
    std::cout << "\n[STATUS CHECK] Перевірка поточного стану випадкових задач за ID:\n";
    {
        std::lock_guard<std::mutex> lock(id_vector_mutex);
        // Візьмемо для прикладу першу, десяту та тридцяту задачі, якщо вони існують
        size_t samples[] = { 1, 10, 30 };
        for (size_t sample_id : samples) {
            if (sample_id <= tracking_ids.size()) {
                TaskStatus status = pool.get_task_status(sample_id);
                std::cout << "  - Задача з ID #" << sample_id << " має статус: ";
                switch (status) {
                case TaskStatus::Pending:   std::cout << "В черзі (Pending)\n"; break;
                case TaskStatus::Running:   std::cout << "Виконується (Running)\n"; break;
                case TaskStatus::Completed: std::cout << "Успішно виконано (Completed)\n"; break;
                case TaskStatus::Rejected:  std::cout << "Відкинута/Переповнено (Rejected)\n"; break;
                }
            }
        }
    }

    // 6. Плавна зупинка пулу — чекаємо виконання поточних задач у черзі
    std::cout << "\n[SHUTDOWN] Ініціалізація плавного завершення роботи пулу (чекаємо залишок задач)...\n";
    pool.terminate(false);
    std::cout << "[SHUTDOWN] Пул потоків повністю зупинено.\n";

    // 7. Виведення фінального розрахунку зібраних метрик у консоль для звіту
    pool.print_metrics();

    return 0;
}