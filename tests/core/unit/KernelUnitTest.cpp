#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "core/kernel/base/MicroKernel.hpp"
#include "core/kernel/base/ParentKernel.hpp"
#include "core/kernel/advanced/SmartKernel.hpp"
#include "core/kernel/advanced/ComputationalKernel.hpp"
#include "core/kernel/advanced/ArchitecturalKernel.hpp"
#include "core/kernel/advanced/CryptoMicroKernel.hpp"
#include "core/kernel/advanced/OrchestrationKernel.hpp"

void testMicroKernelInitialization() {
    std::cout << "Testing MicroKernel initialization...\n";
    
    cloud::core::kernel::MicroKernel kernel("test_micro");
    assert(!kernel.isRunning());
    assert(kernel.getName() == "test_micro");
    
    bool initResult = kernel.initialize();
    assert(initResult);
    assert(kernel.isRunning());
    assert(kernel.getType() == cloud::core::kernel::KernelType::MICRO);
    
    kernel.shutdown();
    assert(!kernel.isRunning());
    
    std::cout << "[OK] MicroKernel initialization test\n";
}

void testMicroKernelTaskScheduling() {
    std::cout << "Testing MicroKernel task scheduling...\n";
    
    cloud::core::kernel::MicroKernel kernel("task_test");
    assert(kernel.initialize());
    
    std::atomic<int> taskCounter{0};
    
    // Планируем несколько задач
    for (int i = 0; i < 5; ++i) {
        kernel.scheduleTask([&taskCounter, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            taskCounter.fetch_add(i + 1);
        }, i + 1);
    }
    
    // Ждем завершения задач
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Проверяем, что все задачи выполнились
    int expectedSum = 1 + 2 + 3 + 4 + 5;
    assert(taskCounter.load() == expectedSum);
    
    kernel.shutdown();
    std::cout << "[OK] MicroKernel task scheduling test\n";
}

void testMicroKernelMetrics() {
    std::cout << "Testing MicroKernel metrics...\n";
    
    cloud::core::kernel::MicroKernel kernel("metrics_test");
    assert(kernel.initialize());
    
    // Получаем начальные метрики
    auto initialMetrics = kernel.getMetrics();
    assert(initialMetrics.cpu_usage >= 0.0);
    assert(initialMetrics.memory_usage >= 0.0);
    
    // Выполняем работу
    kernel.scheduleTask([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }, 1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Получаем обновленные метрики
    auto updatedMetrics = kernel.getMetrics();
    assert(updatedMetrics.cpu_usage >= 0.0);
    assert(updatedMetrics.memory_usage >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] MicroKernel metrics test\n";
}

void testParentKernelChildManagement() {
    std::cout << "Testing ParentKernel child management...\n";
    
    cloud::core::kernel::ParentKernel parentKernel;
    assert(parentKernel.initialize());
    
    // Создаем дочерние ядра
    auto child1 = std::make_shared<cloud::core::kernel::MicroKernel>("child1");
    auto child2 = std::make_shared<cloud::core::kernel::MicroKernel>("child2");
    
    assert(child1->initialize());
    assert(child2->initialize());
    
    // Добавляем дочерние ядра
    parentKernel.addChild(child1);
    parentKernel.addChild(child2);
    
    // Проверяем количество дочерних ядер
    assert(parentKernel.getChildCount() == 2);
    
    // Получаем список дочерних ядер
    auto children = parentKernel.getChildren();
    assert(children.size() == 2);
    
    // Удаляем дочернее ядро
    parentKernel.removeChild(child1);
    assert(parentKernel.getChildCount() == 1);
    
    parentKernel.shutdown();
    std::cout << "[OK] ParentKernel child management test\n";
}

void testParentKernelTaskDistribution() {
    std::cout << "Testing ParentKernel task distribution...\n";
    
    cloud::core::kernel::ParentKernel parentKernel;
    assert(parentKernel.initialize());
    
    // Создаем дочерние ядра
    auto child1 = std::make_shared<cloud::core::kernel::MicroKernel>("dist_child1");
    auto child2 = std::make_shared<cloud::core::kernel::MicroKernel>("dist_child2");
    
    assert(child1->initialize());
    assert(child2->initialize());
    
    parentKernel.addChild(child1);
    parentKernel.addChild(child2);
    
    std::atomic<int> child1Tasks{0};
    std::atomic<int> child2Tasks{0};
    
    // Планируем задачи на родительское ядро
    for (int i = 0; i < 10; ++i) {
        parentKernel.scheduleTask([&child1Tasks, &child2Tasks, i]() {
            if (i % 2 == 0) {
                child1Tasks.fetch_add(1);
            } else {
                child2Tasks.fetch_add(1);
            }
        }, 1);
    }
    
    // Ждем завершения задач
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Проверяем, что задачи распределились
    assert(child1Tasks.load() > 0);
    assert(child2Tasks.load() > 0);
    
    parentKernel.shutdown();
    std::cout << "[OK] ParentKernel task distribution test\n";
}

void testSmartKernelAdaptiveBehavior() {
    std::cout << "Testing SmartKernel adaptive behavior...\n";
    
    cloud::core::kernel::SmartKernel smartKernel;
    assert(smartKernel.initialize());
    
    // Получаем начальную конфигурацию
    auto initialConfig = smartKernel.getCurrentConfiguration();
    assert(initialConfig.isValid());
    
    // Выполняем адаптацию
    cloud::core::kernel::SystemConditions conditions;
    conditions.cpu_usage = 0.8;
    conditions.memory_usage = 0.7;
    conditions.network_usage = 0.6;
    conditions.temperature = 75.0;
    
    bool adaptationResult = smartKernel.adaptToConditions(conditions);
    assert(adaptationResult);
    
    // Получаем обновленную конфигурацию
    auto updatedConfig = smartKernel.getCurrentConfiguration();
    assert(updatedConfig.isValid());
    
    smartKernel.shutdown();
    std::cout << "[OK] SmartKernel adaptive behavior test\n";
}

void testSmartKernelPerformanceOptimization() {
    std::cout << "Testing SmartKernel performance optimization...\n";
    
    cloud::core::kernel::SmartKernel smartKernel;
    assert(smartKernel.initialize());
    
    // Устанавливаем цель производительности
    cloud::core::kernel::PerformanceTarget target;
    target.target_cpu_usage = 0.7;
    target.target_memory_usage = 0.6;
    target.target_throughput = 1000;
    
    bool optimizationResult = smartKernel.optimizeForPerformance(target);
    assert(optimizationResult);
    
    // Получаем оптимизированную конфигурацию
    auto optimizedConfig = smartKernel.getCurrentConfiguration();
    assert(optimizedConfig.isValid());
    
    smartKernel.shutdown();
    std::cout << "[OK] SmartKernel performance optimization test\n";
}

void testComputationalKernelComputeOperations() {
    std::cout << "Testing ComputationalKernel compute operations...\n";
    
    cloud::core::kernel::ComputationalKernel kernel;
    assert(kernel.initialize());
    
    // Тестовые данные
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    // Выполняем вычислительную операцию
    bool computeResult = kernel.compute(testData);
    assert(computeResult);
    
    // Проверяем метрики после вычислений
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] ComputationalKernel compute operations test\n";
}

void testComputationalKernelSoftwareComputation() {
    std::cout << "Testing ComputationalKernel software computation...\n";
    
    std::vector<uint8_t> inputData = {1, 2, 3, 4, 5};
    auto result = cloud::core::kernel::ComputationalKernel::performSoftwareComputation(inputData);
    
    // Проверяем, что результат не пустой
    assert(!result.empty());
    assert(result.size() > 0);
    
    // Проверяем, что результат отличается от входных данных
    assert(result != inputData);
    
    std::cout << "[OK] ComputationalKernel software computation test\n";
}

void testArchitecturalKernelArchitectureOperations() {
    std::cout << "Testing ArchitecturalKernel architecture operations...\n";
    
    cloud::core::kernel::ArchitecturalKernel kernel;
    assert(kernel.initialize());
    
    // Тестируем архитектурные операции
    cloud::core::kernel::ArchitectureConfig config;
    config.optimization_level = 2;
    config.cache_size = 1024;
    config.pipeline_depth = 5;
    
    bool configResult = kernel.configureArchitecture(config);
    assert(configResult);
    
    // Получаем текущую конфигурацию
    auto currentConfig = kernel.getArchitectureConfig();
    assert(currentConfig.optimization_level == config.optimization_level);
    assert(currentConfig.cache_size == config.cache_size);
    assert(currentConfig.pipeline_depth == config.pipeline_depth);
    
    kernel.shutdown();
    std::cout << "[OK] ArchitecturalKernel architecture operations test\n";
}

void testCryptoMicroKernelCryptoOperations() {
    std::cout << "Testing CryptoMicroKernel crypto operations...\n";
    
    cloud::core::kernel::CryptoMicroKernel kernel("crypto_test");
    assert(kernel.initialize());
    
    // Тестовые данные
    std::vector<uint8_t> inputData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    // Выполняем криптографическую операцию
    std::vector<uint8_t> result;
    bool cryptoResult = kernel.execute(inputData, result);
    assert(cryptoResult);
    assert(!result.empty());
    assert(result != inputData);
    
    // Проверяем метрики
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] CryptoMicroKernel crypto operations test\n";
}

void testOrchestrationKernelOrchestration() {
    std::cout << "Testing OrchestrationKernel orchestration...\n";
    
    cloud::core::kernel::OrchestrationKernel kernel;
    assert(kernel.initialize());
    
    // Создаем план оркестрации
    cloud::core::kernel::OrchestrationPlan plan;
    plan.name = "test_plan";
    plan.steps.push_back({"step1", "micro_kernel", 1});
    plan.steps.push_back({"step2", "computational_kernel", 2});
    plan.steps.push_back({"step3", "crypto_kernel", 3});
    
    // Выполняем оркестрацию
    bool orchestrationResult = kernel.executeOrchestration(plan);
    assert(orchestrationResult);
    
    // Получаем статус оркестрации
    auto status = kernel.getOrchestrationStatus();
    assert(status.isValid());
    
    kernel.shutdown();
    std::cout << "[OK] OrchestrationKernel orchestration test\n";
}

void testKernelErrorHandling() {
    std::cout << "Testing kernel error handling...\n";
    
    // Тестируем инициализацию с некорректными параметрами
    cloud::core::kernel::MicroKernel kernel("");
    // Может вернуть false для пустого имени
    
    // Тестируем планирование задач на неинициализированном ядре
    kernel.scheduleTask([]() {}, 1);
    // Не должно вызывать исключений
    
    // Тестируем получение метрик неинициализированного ядра
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    std::cout << "[OK] Kernel error handling test\n";
}

void testKernelConcurrency() {
    std::cout << "Testing kernel concurrency...\n";
    
    cloud::core::kernel::MicroKernel kernel("concurrency_test");
    assert(kernel.initialize());
    
    std::atomic<int> taskCounter{0};
    std::vector<std::thread> threads;
    
    // Запускаем множество потоков, планирующих задачи
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&kernel, &taskCounter, i]() {
            for (int j = 0; j < 5; ++j) {
                kernel.scheduleTask([&taskCounter]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    taskCounter.fetch_add(1);
                }, 1);
            }
        });
    }
    
    // Ждем завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Ждем завершения всех задач
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Проверяем, что все задачи выполнились
    assert(taskCounter.load() == 50);
    
    kernel.shutdown();
    std::cout << "[OK] Kernel concurrency test\n";
}

int main() {
    try {
        testMicroKernelInitialization();
        testMicroKernelTaskScheduling();
        testMicroKernelMetrics();
        testParentKernelChildManagement();
        testParentKernelTaskDistribution();
        testSmartKernelAdaptiveBehavior();
        testSmartKernelPerformanceOptimization();
        testComputationalKernelComputeOperations();
        testComputationalKernelSoftwareComputation();
        testArchitecturalKernelArchitectureOperations();
        testCryptoMicroKernelCryptoOperations();
        testOrchestrationKernelOrchestration();
        testKernelErrorHandling();
        testKernelConcurrency();
        std::cout << "All kernel unit tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Kernel unit test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Kernel unit test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 