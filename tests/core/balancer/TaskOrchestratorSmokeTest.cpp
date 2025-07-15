#include <cassert>
#include <iostream>
#include <thread>
#include <memory>
#include "core/balancer/TaskOrchestrator.hpp"

void smokeTestTaskOrchestrator() {
    std::cout << "Testing TaskOrchestrator basic operations...\n";
    
    cloud::core::balancer::TaskOrchestrator orchestrator;
    
    // Проверяем начальное состояние
    assert(orchestrator.queueSize() == 0);
    assert(orchestrator.getOrchestrationPolicy() == "fifo");
    
    std::cout << "[OK] TaskOrchestrator smoke test\n";
}

void testTaskOrchestratorEnqueueDequeue() {
    std::cout << "Testing TaskOrchestrator enqueue/dequeue operations...\n";
    
    cloud::core::balancer::TaskOrchestrator orchestrator;
    
    // Добавляем задачи
    std::vector<uint8_t> task1 = {1, 2, 3};
    std::vector<uint8_t> task2 = {4, 5, 6};
    std::vector<uint8_t> task3 = {7, 8, 9};
    
    orchestrator.enqueueTask(task1);
    assert(orchestrator.queueSize() == 1);
    
    orchestrator.enqueueTask(task2);
    assert(orchestrator.queueSize() == 2);
    
    orchestrator.enqueueTask(task3);
    assert(orchestrator.queueSize() == 3);
    
    // Извлекаем задачи
    std::vector<uint8_t> dequeuedTask;
    bool success = orchestrator.dequeueTask(dequeuedTask);
    assert(success);
    assert(dequeuedTask == task1);
    assert(orchestrator.queueSize() == 2);
    
    success = orchestrator.dequeueTask(dequeuedTask);
    assert(success);
    assert(dequeuedTask == task2);
    assert(orchestrator.queueSize() == 1);
    
    success = orchestrator.dequeueTask(dequeuedTask);
    assert(success);
    assert(dequeuedTask == task3);
    assert(orchestrator.queueSize() == 0);
    
    // Попытка извлечь из пустой очереди
    success = orchestrator.dequeueTask(dequeuedTask);
    assert(!success);
    
    std::cout << "[OK] TaskOrchestrator enqueue/dequeue test\n";
}

void testTaskOrchestratorPolicyManagement() {
    std::cout << "Testing TaskOrchestrator policy management...\n";
    
    cloud::core::balancer::TaskOrchestrator orchestrator;
    
    // Проверяем политику по умолчанию
    assert(orchestrator.getOrchestrationPolicy() == "fifo");
    
    // Устанавливаем новую политику
    orchestrator.setOrchestrationPolicy("priority");
    assert(orchestrator.getOrchestrationPolicy() == "priority");
    
    // Устанавливаем другую политику
    orchestrator.setOrchestrationPolicy("round_robin");
    assert(orchestrator.getOrchestrationPolicy() == "round_robin");
    
    // Возвращаем к FIFO
    orchestrator.setOrchestrationPolicy("fifo");
    assert(orchestrator.getOrchestrationPolicy() == "fifo");
    
    std::cout << "[OK] TaskOrchestrator policy management test\n";
}

void testTaskOrchestratorQueueSize() {
    std::cout << "Testing TaskOrchestrator queue size management...\n";
    
    cloud::core::balancer::TaskOrchestrator orchestrator;
    
    // Проверяем пустую очередь
    assert(orchestrator.queueSize() == 0);
    
    // Добавляем задачи и проверяем размер
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> task = {static_cast<uint8_t>(i)};
        orchestrator.enqueueTask(task);
        assert(orchestrator.queueSize() == i + 1);
    }
    
    // Извлекаем задачи и проверяем размер
    for (int i = 10; i > 0; --i) {
        std::vector<uint8_t> task;
        bool success = orchestrator.dequeueTask(task);
        assert(success);
        assert(orchestrator.queueSize() == i - 1);
    }
    
    // Проверяем, что очередь пуста
    assert(orchestrator.queueSize() == 0);
    
    std::cout << "[OK] TaskOrchestrator queue size test\n";
}

void testTaskOrchestratorStressTest() {
    std::cout << "Testing TaskOrchestrator stress operations...\n";
    
    cloud::core::balancer::TaskOrchestrator orchestrator;
    
    // Добавляем много задач
    const int numTasks = 1000;
    for (int i = 0; i < numTasks; ++i) {
        std::vector<uint8_t> task = {static_cast<uint8_t>(i % 256)};
        orchestrator.enqueueTask(task);
    }
    
    assert(orchestrator.queueSize() == numTasks);
    
    // Извлекаем все задачи
    int extractedCount = 0;
    while (orchestrator.queueSize() > 0) {
        std::vector<uint8_t> task;
        if (orchestrator.dequeueTask(task)) {
            extractedCount++;
        }
    }
    
    assert(extractedCount == numTasks);
    assert(orchestrator.queueSize() == 0);
    
    std::cout << "[OK] TaskOrchestrator stress test\n";
}

void testTaskOrchestratorConcurrentAccess() {
    std::cout << "Testing TaskOrchestrator concurrent access...\n";
    
    cloud::core::balancer::TaskOrchestrator orchestrator;
    
    // Добавляем задачи из нескольких потоков
    std::vector<std::thread> threads;
    const int numThreads = 4;
    const int tasksPerThread = 25;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&orchestrator, t, tasksPerThread]() {
            for (int i = 0; i < tasksPerThread; ++i) {
                std::vector<uint8_t> task = {static_cast<uint8_t>(t * tasksPerThread + i)};
                orchestrator.enqueueTask(task);
            }
        });
    }
    
    // Ждем завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Проверяем, что все задачи добавлены
    assert(orchestrator.queueSize() == numThreads * tasksPerThread);
    
    // Извлекаем все задачи
    int extractedCount = 0;
    while (orchestrator.queueSize() > 0) {
        std::vector<uint8_t> task;
        if (orchestrator.dequeueTask(task)) {
            extractedCount++;
        }
    }
    
    assert(extractedCount == numThreads * tasksPerThread);
    
    std::cout << "[OK] TaskOrchestrator concurrent access test\n";
}

int main() {
    try {
        smokeTestTaskOrchestrator();
        testTaskOrchestratorEnqueueDequeue();
        testTaskOrchestratorPolicyManagement();
        testTaskOrchestratorQueueSize();
        testTaskOrchestratorStressTest();
        testTaskOrchestratorConcurrentAccess();
        std::cout << "All TaskOrchestrator tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "TaskOrchestrator test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "TaskOrchestrator test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 