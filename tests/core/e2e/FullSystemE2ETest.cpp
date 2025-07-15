#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include "core/kernel/base/MicroKernel.hpp"
#include "core/kernel/base/ParentKernel.hpp"
#include "core/kernel/advanced/SmartKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/manager/CacheManager.hpp"
#include "core/security/SecurityManager.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/balancer/TaskTypes.hpp"

class FullSystemE2ETest {
private:
    std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer;
    std::shared_ptr<cloud::core::cache::CacheManager> cacheManager;
    std::shared_ptr<cloud::core::security::SecurityManager> securityManager;
    std::shared_ptr<cloud::core::recovery::RecoveryManager> recoveryManager;
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool;
    std::shared_ptr<cloud::core::drivers::ARMDriver> armDriver;
    
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels;
    
public:
    FullSystemE2ETest() {
        initializeComponents();
    }
    
    ~FullSystemE2ETest() {
        shutdownComponents();
    }
    
    void initializeComponents() {
        std::cout << "Initializing full system components...\n";
        
        // Инициализация LoadBalancer
        loadBalancer = std::make_shared<cloud::core::balancer::LoadBalancer>();
        
        // Инициализация CacheManager
        cloud::core::cache::CacheConfig cacheConfig;
        cacheConfig.maxSize = 1024 * 1024 * 50; // 50MB
        cacheConfig.maxEntries = 2000;
        cacheConfig.enableMetrics = true;
        cacheManager = std::make_shared<cloud::core::cache::CacheManager>(cacheConfig);
        assert(cacheManager->initialize());
        
        // Инициализация SecurityManager
        securityManager = std::make_shared<cloud::core::security::SecurityManager>();
        assert(securityManager->initialize());
        
        // Инициализация RecoveryManager
        cloud::core::recovery::RecoveryConfig recoveryConfig;
        recoveryConfig.maxRecoveryPoints = 10;
        recoveryConfig.checkpointInterval = std::chrono::seconds(30);
        recoveryConfig.enableAutoRecovery = true;
        recoveryConfig.enableStateValidation = false;
        recoveryConfig.pointConfig.maxSize = 1024 * 1024; // 1MB
        recoveryConfig.pointConfig.enableCompression = false;
        recoveryConfig.pointConfig.storagePath = "./e2e_recovery_points";
        recoveryConfig.pointConfig.retentionPeriod = std::chrono::seconds(3600); // 1 hour
        
        // Создаем директорию для recovery points
        std::filesystem::create_directories("./e2e_recovery_points");
        
        recoveryManager = std::make_shared<cloud::core::recovery::RecoveryManager>(recoveryConfig);
        assert(recoveryManager->initialize());
        
        // Инициализация ThreadPool
        cloud::core::thread::ThreadPoolConfig threadConfig;
        threadConfig.minThreads = 4;
        threadConfig.maxThreads = 8;
        threadConfig.queueSize = 100;
        threadConfig.stackSize = 1024 * 1024; // 1MB
        threadPool = std::make_shared<cloud::core::thread::ThreadPool>(threadConfig);
        
        // Инициализация ARMDriver
        armDriver = std::make_shared<cloud::core::drivers::ARMDriver>();
        assert(armDriver->initialize());
        
        // Инициализация ядер
        initializeKernels();
        
        std::cout << "All components initialized successfully\n";
    }
    
    void initializeKernels() {
        // Создаем различные типы ядер
        auto microKernel1 = std::make_shared<cloud::core::kernel::MicroKernel>("e2e_micro_1");
        auto microKernel2 = std::make_shared<cloud::core::kernel::MicroKernel>("e2e_micro_2");
        auto parentKernel = std::make_shared<cloud::core::kernel::ParentKernel>();
        auto smartKernel = std::make_shared<cloud::core::kernel::SmartKernel>();
        
        // Инициализируем ядра
        assert(microKernel1->initialize());
        assert(microKernel2->initialize());
        assert(parentKernel->initialize());
        assert(smartKernel->initialize());
        
        // Добавляем дочерние ядра к родительскому
        parentKernel->addChild(microKernel1);
        parentKernel->addChild(microKernel2);
        
        // Добавляем ядра в вектор
        kernels = {parentKernel, smartKernel};
    }
    
    void shutdownComponents() {
        std::cout << "Shutting down full system components...\n";
        
        // Завершаем работу ядер
        for (auto& kernel : kernels) {
            kernel->shutdown();
        }
        
        // Завершаем работу компонентов
        if (cacheManager) cacheManager->shutdown();
        if (securityManager) securityManager->shutdown();
        if (recoveryManager) recoveryManager->shutdown();
        
        std::cout << "All components shut down successfully\n";
    }
    
    void testFullWorkflow() {
        std::cout << "Testing full system workflow...\n";
        
        // 1. Устанавливаем политику безопасности
        securityManager->setPolicy("secure");
        assert(securityManager->getPolicy() == "secure");
        
        // 2. Добавляем данные в кэш
        std::vector<uint8_t> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        assert(cacheManager->putData("e2e_test_key", testData));
        
        // 3. Создаем задачи для балансировки
        std::vector<cloud::core::balancer::TaskDescriptor> tasks;
        for (int i = 0; i < 10; ++i) {
            cloud::core::balancer::TaskDescriptor task;
            task.data = std::vector<uint8_t>(100, i % 256);
            task.priority = i % 10;
            task.type = static_cast<cloud::core::balancer::TaskType>(i % 5);
            task.enqueueTime = std::chrono::steady_clock::now();
            tasks.push_back(task);
        }
        
        // 4. Создаем метрики ядер
        std::vector<cloud::core::balancer::KernelMetrics> metrics;
        for (size_t i = 0; i < kernels.size(); ++i) {
            cloud::core::balancer::KernelMetrics metric;
            metric.cpuUsage = 0.1 + (i * 0.3);
            metric.memoryUsage = 0.05 + (i * 0.25);
            metric.networkBandwidth = 500.0 + (i * 300.0);
            metric.diskIO = 800.0 + (i * 400.0);
            metric.energyConsumption = 30.0 + (i * 25.0);
            metric.cpuTaskEfficiency = 0.6 + (i * 0.2);
            metric.ioTaskEfficiency = 0.5 + (i * 0.25);
            metric.memoryTaskEfficiency = 0.7 + (i * 0.2);
            metric.networkTaskEfficiency = 0.8 + (i * 0.15);
            metrics.push_back(metric);
        }
        
        // 5. Выполняем балансировку нагрузки
        loadBalancer->balance(kernels, tasks, metrics);
        
        // 6. Создаем точку восстановления
        std::string recoveryPointId = recoveryManager->createRecoveryPoint();
        assert(!recoveryPointId.empty());
        
        // 7. Выполняем задачи в пуле потоков
        std::atomic<int> completedTasks{0};
        for (int i = 0; i < 5; ++i) {
            threadPool->enqueue([&completedTasks, i]() {
                completedTasks++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            });
        }
        
        // 8. Ждем завершения задач
        threadPool->waitForCompletion();
        assert(completedTasks == 5);
        
        // 9. Получаем данные из кэша
        std::vector<uint8_t> retrievedData;
        assert(cacheManager->getData("e2e_test_key", retrievedData));
        assert(retrievedData == testData);
        
        // Обновляем метрики кэша
        cacheManager->updateMetrics();
        
        // 10. Аудит всех операций
        securityManager->auditEvent("e2e_workflow", "workflow_completed");
        securityManager->auditEvent("recovery_point", recoveryPointId);
        
        // 11. Проверяем метрики всех компонентов
        auto cacheMetrics = cacheManager->getMetrics();
        assert(cacheMetrics.entryCount > 0);
        
        auto recoveryMetrics = recoveryManager->getMetrics();
        assert(recoveryMetrics.totalPoints > 0);
        
        auto threadMetrics = threadPool->getMetrics();
        assert(threadMetrics.totalThreads > 0);
        
        // 12. Проверяем, что все ядра работают
        for (const auto& kernel : kernels) {
            assert(kernel->isRunning());
        }
        
        std::cout << "[OK] Full system workflow test completed\n";
    }
    
    void testSystemRecovery() {
        std::cout << "Testing system recovery...\n";
        
        // 1. Создаем начальную точку восстановления
        std::string initialPointId = recoveryManager->createRecoveryPoint();
        assert(!initialPointId.empty());
        
        // 2. Выполняем некоторые операции
        std::vector<uint8_t> data = {10, 20, 30, 40, 50};
        cacheManager->putData("recovery_test_key", data);
        
        // 3. Создаем промежуточную точку восстановления
        std::string intermediatePointId = recoveryManager->createRecoveryPoint();
        assert(!intermediatePointId.empty());
        
        // 4. Выполняем еще операции
        cacheManager->putData("recovery_test_key_2", data);
        
        // 5. Восстанавливаемся из промежуточной точки
        bool restoreSuccess = recoveryManager->restoreFromPoint(intermediatePointId);
        assert(restoreSuccess);
        
        // 6. Проверяем состояние после восстановления
        std::vector<uint8_t> retrievedData;
        assert(cacheManager->getData("recovery_test_key", retrievedData));
        assert(retrievedData == data);
        
        // 7. Аудит операции восстановления
        securityManager->auditEvent("system_recovery", intermediatePointId);
        
        std::cout << "[OK] System recovery test completed\n";
    }
    
    void testSystemStress() {
        std::cout << "Testing system stress...\n";
        
        const int numOperations = 100;
        std::atomic<int> completedOperations{0};
        
        // Выполняем много операций параллельно
        for (int i = 0; i < numOperations; ++i) {
            threadPool->enqueue([this, i, &completedOperations]() {
                // Кэш операция
                std::vector<uint8_t> data(50, i % 256);
                cacheManager->putData("stress_key_" + std::to_string(i), data);
                
                // Планируем задачу на ядро
                if (!kernels.empty()) {
                    kernels[0]->scheduleTask([]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }, i % 5);
                }
                
                completedOperations++;
            });
        }
        
        // Ждем завершения всех операций
        threadPool->waitForCompletion();
        assert(completedOperations == numOperations);
        
        // Создаем финальную точку восстановления
        std::string stressPointId = recoveryManager->createRecoveryPoint();
        assert(!stressPointId.empty());
        
        // Обновляем метрики кэша
        cacheManager->updateMetrics();
        
        // Проверяем метрики
        auto cacheMetrics = cacheManager->getMetrics();
        assert(cacheMetrics.entryCount > 0);
        
        auto recoveryMetrics = recoveryManager->getMetrics();
        assert(recoveryMetrics.totalPoints > 0);
        
        // Проверяем, что все ядра работают
        for (const auto& kernel : kernels) {
            assert(kernel->isRunning());
        }
        
        std::cout << "[OK] System stress test completed\n";
    }
    
    void testSystemErrorHandling() {
        std::cout << "Testing system error handling...\n";
        
        // 1. Тестируем обработку несуществующих данных
        std::vector<uint8_t> nonExistentData;
        bool getSuccess = cacheManager->getData("non_existent_key", nonExistentData);
        assert(!getSuccess);
        
        // 2. Тестируем восстановление из несуществующей точки
        bool restoreSuccess = recoveryManager->restoreFromPoint("non_existent_point");
        // Может вернуть false для несуществующей точки
        
        // 3. Аудит ошибок
        securityManager->auditEvent("error_handling", "non_existent_data_access");
        securityManager->auditEvent("error_handling", "non_existent_recovery_point");
        
        // 4. Проверяем, что система продолжает работать
        assert(cacheManager->putData("error_test_key", {1, 2, 3}));
        
        std::string validPointId = recoveryManager->createRecoveryPoint();
        assert(!validPointId.empty());
        
        // 5. Проверяем, что все ядра работают
        for (const auto& kernel : kernels) {
            assert(kernel->isRunning());
        }
        
        std::cout << "[OK] System error handling test completed\n";
    }
    
    void runAllTests() {
        std::cout << "Running all E2E tests...\n";
        
        testFullWorkflow();
        testSystemRecovery();
        testSystemStress();
        testSystemErrorHandling();
        
        std::cout << "All E2E tests completed successfully!\n";
    }
};

int main() {
    try {
        FullSystemE2ETest e2eTest;
        e2eTest.runAllTests();
        std::cout << "Full system E2E test suite passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "E2E test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "E2E test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 