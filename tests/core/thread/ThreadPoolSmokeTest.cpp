#include <cassert>
#include <iostream>
#include <memory>
#include "core/thread/ThreadPool.hpp"

void smokeTestThreadPool() {
    std::cout << "Testing ThreadPool basic operations...\n";
    
    cloud::core::thread::ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 8;
    config.queueSize = 100;
    config.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(config);
    
    // Проверяем начальное состояние
    assert(pool.getActiveThreadCount() >= 0);
    assert(pool.getQueueSize() == 0);
    assert(pool.isQueueEmpty());
    
    std::cout << "[OK] ThreadPool smoke test\n";
}

void testThreadPoolTaskExecution() {
    std::cout << "Testing ThreadPool task execution...\n";
    
    cloud::core::thread::ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.queueSize = 50;
    config.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(config);
    
    std::atomic<int> taskCounter{0};
    
    // Добавляем задачи
    for (int i = 0; i < 5; ++i) {
        pool.enqueue([&taskCounter, i]() {
            taskCounter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    
    // Ждем завершения всех задач
    pool.waitForCompletion();
    
    assert(taskCounter == 5);
    
    std::cout << "[OK] ThreadPool task execution test\n";
}

void testThreadPoolQueueManagement() {
    std::cout << "Testing ThreadPool queue management...\n";
    
    cloud::core::thread::ThreadPoolConfig config;
    config.minThreads = 1;
    config.maxThreads = 2;
    config.queueSize = 10;
    config.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(config);
    
    // Проверяем пустую очередь
    assert(pool.isQueueEmpty());
    assert(pool.getQueueSize() == 0);
    
    // Добавляем задачи
    for (int i = 0; i < 3; ++i) {
        pool.enqueue([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });
    }
    
    // Проверяем размер очереди
    assert(pool.getQueueSize() >= 0);
    
    // Ждем завершения
    pool.waitForCompletion();
    
    // Проверяем, что очередь пуста
    assert(pool.isQueueEmpty());
    
    std::cout << "[OK] ThreadPool queue management test\n";
}

void testThreadPoolMetrics() {
    std::cout << "Testing ThreadPool metrics collection...\n";
    
    cloud::core::thread::ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.queueSize = 20;
    config.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(config);
    
    // Получаем начальные метрики
    auto initialMetrics = pool.getMetrics();
    assert(initialMetrics.totalThreads >= config.minThreads);
    assert(initialMetrics.totalThreads <= config.maxThreads);
    
    // Добавляем задачи для генерации метрик
    std::atomic<int> completedTasks{0};
    for (int i = 0; i < 5; ++i) {
        pool.enqueue([&completedTasks]() {
            completedTasks++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    
    // Обновляем метрики
    pool.updateMetrics();
    
    // Получаем обновленные метрики
    auto updatedMetrics = pool.getMetrics();
    assert(updatedMetrics.totalThreads >= config.minThreads);
    assert(updatedMetrics.totalThreads <= config.maxThreads);
    
    // Ждем завершения задач
    pool.waitForCompletion();
    assert(completedTasks == 5);
    
    std::cout << "[OK] ThreadPool metrics test\n";
}

void testThreadPoolConfiguration() {
    std::cout << "Testing ThreadPool configuration management...\n";
    
    cloud::core::thread::ThreadPoolConfig initialConfig;
    initialConfig.minThreads = 2;
    initialConfig.maxThreads = 4;
    initialConfig.queueSize = 50;
    initialConfig.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(initialConfig);
    
    // Проверяем текущую конфигурацию
    auto currentConfig = pool.getConfiguration();
    assert(currentConfig.minThreads == 2);
    assert(currentConfig.maxThreads == 4);
    assert(currentConfig.queueSize == 50);
    assert(currentConfig.stackSize == 1024 * 1024);
    
    // Устанавливаем новую конфигурацию
    cloud::core::thread::ThreadPoolConfig newConfig;
    newConfig.minThreads = 3;
    newConfig.maxThreads = 6;
    newConfig.queueSize = 100;
    newConfig.stackSize = 2 * 1024 * 1024; // 2MB
    
    pool.setConfiguration(newConfig);
    
    // Проверяем обновленную конфигурацию
    auto updatedConfig = pool.getConfiguration();
    assert(updatedConfig.minThreads == 3);
    assert(updatedConfig.maxThreads == 6);
    assert(updatedConfig.queueSize == 100);
    assert(updatedConfig.stackSize == 2 * 1024 * 1024);
    
    std::cout << "[OK] ThreadPool configuration test\n";
}

void testThreadPoolStopRestart() {
    std::cout << "Testing ThreadPool stop/restart operations...\n";
    
    cloud::core::thread::ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.queueSize = 20;
    config.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(config);
    
    // Добавляем несколько задач
    std::atomic<int> taskCounter{0};
    for (int i = 0; i < 3; ++i) {
        pool.enqueue([&taskCounter]() {
            taskCounter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    
    // Останавливаем пул
    pool.stop();
    
    // Перезапускаем пул
    pool.restart();
    
    // Добавляем еще задачи
    for (int i = 0; i < 2; ++i) {
        pool.enqueue([&taskCounter]() {
            taskCounter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    
    // Ждем завершения
    pool.waitForCompletion();
    
    assert(taskCounter >= 3); // Минимум первые 3 задачи должны выполниться
    
    std::cout << "[OK] ThreadPool stop/restart test\n";
}

void testThreadPoolStressTest() {
    std::cout << "Testing ThreadPool stress operations...\n";
    
    cloud::core::thread::ThreadPoolConfig config;
    config.minThreads = 4;
    config.maxThreads = 8;
    config.queueSize = 200;
    config.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(config);
    
    std::atomic<int> completedTasks{0};
    const int numTasks = 100;
    
    // Добавляем много задач
    for (int i = 0; i < numTasks; ++i) {
        pool.enqueue([&completedTasks, i]() {
            completedTasks++;
            // Симулируем работу
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });
    }
    
    // Ждем завершения всех задач
    pool.waitForCompletion();
    
    assert(completedTasks == numTasks);
    
    // Проверяем метрики
    auto metrics = pool.getMetrics();
    assert(metrics.totalThreads >= config.minThreads);
    assert(metrics.totalThreads <= config.maxThreads);
    
    std::cout << "[OK] ThreadPool stress test\n";
}

void testThreadPoolConcurrentAccess() {
    std::cout << "Testing ThreadPool concurrent access...\n";
    
    cloud::core::thread::ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.queueSize = 100;
    config.stackSize = 1024 * 1024; // 1MB
    
    cloud::core::thread::ThreadPool pool(config);
    
    std::atomic<int> taskCounter{0};
    const int numThreads = 4;
    const int tasksPerThread = 25;
    
    // Создаем потоки, которые добавляют задачи
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&pool, &taskCounter, t, tasksPerThread]() {
            for (int i = 0; i < tasksPerThread; ++i) {
                pool.enqueue([&taskCounter]() {
                    taskCounter++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                });
            }
        });
    }
    
    // Ждем завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Ждем завершения всех задач
    pool.waitForCompletion();
    
    assert(taskCounter == numThreads * tasksPerThread);
    
    std::cout << "[OK] ThreadPool concurrent access test\n";
}

int main() {
    try {
        smokeTestThreadPool();
        testThreadPoolTaskExecution();
        testThreadPoolQueueManagement();
        testThreadPoolMetrics();
        testThreadPoolConfiguration();
        testThreadPoolStopRestart();
        testThreadPoolStressTest();
        testThreadPoolConcurrentAccess();
        std::cout << "All ThreadPool tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "ThreadPool test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ThreadPool test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 