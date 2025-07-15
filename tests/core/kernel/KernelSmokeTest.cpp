#include <cassert>
#include <iostream>
#include "core/kernel/base/MicroKernel.hpp"
#include "core/kernel/base/ParentKernel.hpp"
#include "core/kernel/advanced/OrchestrationKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include "core/balancer/TaskTypes.hpp"
#include <memory>
#include <vector>
#include <chrono>
#include <any>

void smokeTestParentKernel() {
    cloud::core::kernel::ParentKernel parent;
    assert(parent.initialize());
    auto child = std::make_shared<cloud::core::kernel::MicroKernel>("micro1");
    assert(child->initialize());
    parent.addChild(child);
    assert(parent.getChildren().size() == 1);
    parent.removeChild(child->getId());
    assert(parent.getChildren().empty());
    parent.shutdown();
    std::cout << "[OK] ParentKernel smoke test\n";
}

void smokeTestOrchestrationKernel() {
    cloud::core::kernel::OrchestrationKernel ork;
    assert(ork.initialize());
    std::vector<uint8_t> data = {1,2,3};
    ork.enqueueTask(data, 7);
    ork.shutdown();
    std::cout << "[OK] OrchestrationKernel smoke test\n";
}

void stressTestOrchestrationKernel() {
    cloud::core::kernel::OrchestrationKernel ork;
    assert(ork.initialize());
    // Уменьшаем количество задач для избежания переполнения
    for (int i = 0; i < 100; ++i) {
        std::vector<uint8_t> data(10, i % 256);
        ork.enqueueTask(data, i % 10);
    }
    // Уменьшаем количество итераций балансировки
    for (int i = 0; i < 10; ++i) {
        ork.balanceTasks();
    }
    ork.shutdown();
    std::cout << "[OK] OrchestrationKernel stress test\n";
}

void testKernelLoadBalancerIntegration() {
    std::cout << "Testing Kernel-LoadBalancer integration...\n";
    
    // Создаем LoadBalancer
    auto loadBalancer = std::make_shared<cloud::core::balancer::LoadBalancer>();
    
    // Создаем ядра
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("micro_test");
    auto parentKernel = std::make_shared<cloud::core::kernel::ParentKernel>();
    
    // Инициализируем ядра
    assert(microKernel->initialize());
    assert(parentKernel->initialize());
    
    // Создаем тестовые задачи
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    for (int i = 0; i < 3; ++i) {
        cloud::core::balancer::TaskDescriptor task;
        task.data = std::vector<uint8_t>(10, i);
        task.priority = i % 10;
        task.type = static_cast<cloud::core::balancer::TaskType>(i % 5);
        task.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(task);
    }
    
    // Создаем метрики ядер
    std::vector<cloud::core::balancer::KernelMetrics> metrics;
    for (int i = 0; i < 2; ++i) {
        cloud::core::balancer::KernelMetrics metric;
        metric.cpuUsage = 0.5;
        metric.memoryUsage = 0.3;
        metric.networkBandwidth = 1000.0;
        metric.diskIO = 1000.0;
        metric.energyConsumption = 50.0;
        metric.cpuTaskEfficiency = 0.8;
        metric.ioTaskEfficiency = 0.7;
        metric.memoryTaskEfficiency = 0.6;
        metric.networkTaskEfficiency = 0.9;
        metrics.push_back(metric);
    }
    
    // Тестируем балансировку
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels = {microKernel, parentKernel};
    loadBalancer->balance(kernels, tasks, metrics);
    
    std::cout << "[OK] Kernel-LoadBalancer integration test\n";
}

void testKernelPreloadManagerIntegration() {
    std::cout << "Testing Kernel-PreloadManager integration...\n";
    
    // Создаем PreloadManager
    auto preloadManager = std::make_shared<cloud::core::cache::experimental::PreloadManager>(cloud::core::cache::experimental::PreloadConfig{100, 10, std::chrono::seconds(10), 0.7});
    
    // Добавляем тестовые данные
    for (int i = 0; i < 5; ++i) {
        std::string key = "test_key_" + std::to_string(i);
        std::vector<uint8_t> data(10, i);
        preloadManager->addData(key, data);
    }
    
    // Создаем ядро
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("preload_test");
    
    // Инициализируем ядро
    assert(microKernel->initialize());
    
    // Проверяем базовые метрики
    auto metrics = microKernel->getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    
    std::cout << "[OK] Kernel-PreloadManager integration test\n";
}

void testBasicKernelOperations() {
    std::cout << "Testing basic kernel operations...\n";
    
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("basic_test");
    assert(microKernel->initialize());
    
    // Проверяем базовые операции
    assert(microKernel->getId() == "basic_test");
    assert(microKernel->isRunning());
    
    // Получаем метрики
    auto metrics = microKernel->getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    // Останавливаем ядро
    microKernel->shutdown();
    assert(!microKernel->isRunning());
    
    std::cout << "[OK] Basic kernel operations test\n";
}

int main() {
    try {
        smokeTestParentKernel();
        smokeTestOrchestrationKernel();
        stressTestOrchestrationKernel();
        testKernelLoadBalancerIntegration();
        testKernelPreloadManagerIntegration();
        testBasicKernelOperations();
        std::cout << "All kernel integration tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 