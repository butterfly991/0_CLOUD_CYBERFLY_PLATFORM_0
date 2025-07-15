#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include "core/kernel/base/MicroKernel.hpp"
#include "core/kernel/base/ParentKernel.hpp"
#include "core/kernel/advanced/SmartKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/balancer/TaskTypes.hpp"

void testKernelLoadBalancerBasicIntegration() {
    std::cout << "Testing basic Kernel-LoadBalancer integration...\n";
    
    // Создаем LoadBalancer
    auto loadBalancer = std::make_shared<cloud::core::balancer::LoadBalancer>();
    
    // Создаем различные типы ядер
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("micro_integration");
    auto parentKernel = std::make_shared<cloud::core::kernel::ParentKernel>();
    auto smartKernel = std::make_shared<cloud::core::kernel::SmartKernel>();
    
    // Инициализируем ядра
    assert(microKernel->initialize());
    assert(parentKernel->initialize());
    assert(smartKernel->initialize());
    
    // Создаем вектор ядер
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels = {
        microKernel, parentKernel, smartKernel
    };
    
    // Создаем тестовые задачи
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    for (int i = 0; i < 5; ++i) {
        cloud::core::balancer::TaskDescriptor task;
        task.data = std::vector<uint8_t>(50, i);
        task.priority = i % 10;
        task.type = static_cast<cloud::core::balancer::TaskType>(i % 5);
        task.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(task);
    }
    
    // Создаем метрики ядер
    std::vector<cloud::core::balancer::KernelMetrics> metrics;
    for (size_t i = 0; i < kernels.size(); ++i) {
        cloud::core::balancer::KernelMetrics metric;
        metric.cpuUsage = 0.1 + (i * 0.2);
        metric.memoryUsage = 0.05 + (i * 0.15);
        metric.networkBandwidth = 500.0 + (i * 200.0);
        metric.diskIO = 800.0 + (i * 300.0);
        metric.energyConsumption = 30.0 + (i * 20.0);
        metric.cpuTaskEfficiency = 0.6 + (i * 0.1);
        metric.ioTaskEfficiency = 0.5 + (i * 0.15);
        metric.memoryTaskEfficiency = 0.7 + (i * 0.1);
        metric.networkTaskEfficiency = 0.8 + (i * 0.1);
        metrics.push_back(metric);
    }
    
    // Выполняем балансировку
    loadBalancer->balance(kernels, tasks, metrics);
    
    // Проверяем, что ядра все еще работают
    assert(microKernel->isRunning());
    assert(parentKernel->isRunning());
    assert(smartKernel->isRunning());
    
    // Завершаем работу ядер
    microKernel->shutdown();
    parentKernel->shutdown();
    smartKernel->shutdown();
    
    std::cout << "[OK] Kernel-LoadBalancer basic integration test\n";
}

void testKernelLoadBalancerAdvancedIntegration() {
    std::cout << "Testing advanced Kernel-LoadBalancer integration...\n";
    
    // Создаем LoadBalancer с различными стратегиями
    auto loadBalancer = std::make_shared<cloud::core::balancer::LoadBalancer>();
    
    // Создаем ядра с разными конфигурациями
    auto microKernel1 = std::make_shared<cloud::core::kernel::MicroKernel>("micro_1");
    auto microKernel2 = std::make_shared<cloud::core::kernel::MicroKernel>("micro_2");
    auto parentKernel = std::make_shared<cloud::core::kernel::ParentKernel>();
    
    // Инициализируем ядра
    assert(microKernel1->initialize());
    assert(microKernel2->initialize());
    assert(parentKernel->initialize());
    
    // Добавляем дочерние ядра к родительскому
    parentKernel->addChild(microKernel1);
    parentKernel->addChild(microKernel2);
    
    // Создаем вектор ядер
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels = {
        parentKernel
    };
    
    // Создаем разнообразные задачи
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    for (int i = 0; i < 10; ++i) {
        cloud::core::balancer::TaskDescriptor task;
        task.data = std::vector<uint8_t>(100, i % 256);
        task.priority = (i % 3) * 5; // 0, 5, 10
        task.type = static_cast<cloud::core::balancer::TaskType>(i % 5);
        task.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(task);
    }
    
    // Создаем метрики с разными характеристиками
    std::vector<cloud::core::balancer::KernelMetrics> metrics;
    cloud::core::balancer::KernelMetrics metric;
    metric.cpuUsage = 0.3;
    metric.memoryUsage = 0.2;
    metric.networkBandwidth = 1200.0;
    metric.diskIO = 1500.0;
    metric.energyConsumption = 45.0;
    metric.cpuTaskEfficiency = 0.85;
    metric.ioTaskEfficiency = 0.75;
    metric.memoryTaskEfficiency = 0.8;
    metric.networkTaskEfficiency = 0.9;
    metrics.push_back(metric);
    
    // Тестируем разные стратегии балансировки
    std::vector<std::string> strategies = {"hybrid_adaptive", "resource_aware", "workload_specific"};
    
    for (const auto& strategy : strategies) {
        loadBalancer->setStrategy(strategy);
        loadBalancer->balance(kernels, tasks, metrics);
        
        // Проверяем, что ядра продолжают работать
        assert(parentKernel->isRunning());
        assert(microKernel1->isRunning());
        assert(microKernel2->isRunning());
    }
    
    // Завершаем работу
    parentKernel->shutdown();
    
    std::cout << "[OK] Kernel-LoadBalancer advanced integration test\n";
}

void testKernelLoadBalancerStressIntegration() {
    std::cout << "Testing Kernel-LoadBalancer stress integration...\n";
    
    auto loadBalancer = std::make_shared<cloud::core::balancer::LoadBalancer>();
    
    // Создаем много ядер
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels;
    const int numKernels = 8;
    
    for (int i = 0; i < numKernels; ++i) {
        auto kernel = std::make_shared<cloud::core::kernel::MicroKernel>("stress_kernel_" + std::to_string(i));
        assert(kernel->initialize());
        kernels.push_back(kernel);
    }
    
    // Создаем много задач
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    const int numTasks = 100;
    
    for (int i = 0; i < numTasks; ++i) {
        cloud::core::balancer::TaskDescriptor task;
        task.data = std::vector<uint8_t>(200, i % 256);
        task.priority = i % 10;
        task.type = static_cast<cloud::core::balancer::TaskType>(i % 5);
        task.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(task);
    }
    
    // Создаем метрики для всех ядер
    std::vector<cloud::core::balancer::KernelMetrics> metrics;
    for (int i = 0; i < numKernels; ++i) {
        cloud::core::balancer::KernelMetrics metric;
        metric.cpuUsage = 0.1 + (i % 8) * 0.1;
        metric.memoryUsage = 0.05 + (i % 6) * 0.15;
        metric.networkBandwidth = 500.0 + (i % 20) * 50.0;
        metric.diskIO = 800.0 + (i % 15) * 100.0;
        metric.energyConsumption = 30.0 + (i % 12) * 5.0;
        metric.cpuTaskEfficiency = 0.3 + (i % 7) * 0.1;
        metric.ioTaskEfficiency = 0.2 + (i % 9) * 0.08;
        metric.memoryTaskEfficiency = 0.4 + (i % 6) * 0.1;
        metric.networkTaskEfficiency = 0.25 + (i % 8) * 0.09;
        metrics.push_back(metric);
    }
    
    // Выполняем балансировку несколько раз
    for (int round = 0; round < 5; ++round) {
        loadBalancer->balance(kernels, tasks, metrics);
        
        // Проверяем, что все ядра работают
        for (const auto& kernel : kernels) {
            assert(kernel->isRunning());
        }
    }
    
    // Завершаем работу всех ядер
    for (auto& kernel : kernels) {
        kernel->shutdown();
    }
    
    std::cout << "[OK] Kernel-LoadBalancer stress integration test\n";
}

void testKernelLoadBalancerErrorHandling() {
    std::cout << "Testing Kernel-LoadBalancer error handling...\n";
    
    auto loadBalancer = std::make_shared<cloud::core::balancer::LoadBalancer>();
    
    // Создаем ядра
    auto kernel1 = std::make_shared<cloud::core::kernel::MicroKernel>("error_kernel_1");
    auto kernel2 = std::make_shared<cloud::core::kernel::MicroKernel>("error_kernel_2");
    
    assert(kernel1->initialize());
    assert(kernel2->initialize());
    
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels = {kernel1, kernel2};
    
    // Создаем задачи
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    for (int i = 0; i < 3; ++i) {
        cloud::core::balancer::TaskDescriptor task;
        task.data = std::vector<uint8_t>(10, i);
        task.priority = i;
        task.type = static_cast<cloud::core::balancer::TaskType>(i % 3);
        task.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(task);
    }
    
    // Создаем метрики
    std::vector<cloud::core::balancer::KernelMetrics> metrics = {
        {0.5, 0.3, 1000.0, 1000.0, 50.0, 0.8, 0.7, 0.6, 0.9},
        {0.2, 0.1, 800.0, 1200.0, 30.0, 0.6, 0.9, 0.7, 0.8}
    };
    
    // Тестируем с пустыми векторами
    loadBalancer->balance(std::vector<std::shared_ptr<cloud::core::kernel::IKernel>>(), tasks, metrics);
    loadBalancer->balance(kernels, std::vector<cloud::core::balancer::TaskDescriptor>(), metrics);
    loadBalancer->balance(kernels, tasks, std::vector<cloud::core::balancer::KernelMetrics>());
    
    // Проверяем, что ядра все еще работают
    assert(kernel1->isRunning());
    assert(kernel2->isRunning());
    
    // Завершаем работу
    kernel1->shutdown();
    kernel2->shutdown();
    
    std::cout << "[OK] Kernel-LoadBalancer error handling test\n";
}

int main() {
    try {
        testKernelLoadBalancerBasicIntegration();
        testKernelLoadBalancerAdvancedIntegration();
        testKernelLoadBalancerStressIntegration();
        testKernelLoadBalancerErrorHandling();
        std::cout << "All Kernel-LoadBalancer integration tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Kernel-LoadBalancer integration test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Kernel-LoadBalancer integration test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 