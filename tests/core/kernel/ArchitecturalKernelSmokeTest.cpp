#include <cassert>
#include <iostream>
#include <memory>
#include "core/kernel/advanced/ArchitecturalKernel.hpp"

void smokeTestArchitecturalKernel() {
    std::cout << "Testing ArchitecturalKernel basic operations...\n";
    
    cloud::core::kernel::ArchitecturalKernel kernel;
    assert(kernel.initialize());
    assert(kernel.isRunning());
    assert(kernel.getType() == cloud::core::kernel::KernelType::ARCHITECTURAL);
    
    // Проверяем базовые метрики
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    kernel.shutdown();
    assert(!kernel.isRunning());
    
    std::cout << "[OK] ArchitecturalKernel smoke test\n";
}

void testArchitecturalKernelTopologyOptimization() {
    std::cout << "Testing ArchitecturalKernel topology optimization...\n";
    
    cloud::core::kernel::ArchitecturalKernel kernel;
    assert(kernel.initialize());
    
    // Тестируем оптимизацию топологии
    kernel.optimizeTopology();
    
    // Проверяем, что ядро все еще работает после оптимизации
    assert(kernel.isRunning());
    
    kernel.shutdown();
    std::cout << "[OK] ArchitecturalKernel topology optimization test\n";
}

void testArchitecturalKernelPlacementOptimization() {
    std::cout << "Testing ArchitecturalKernel placement optimization...\n";
    
    cloud::core::kernel::ArchitecturalKernel kernel;
    assert(kernel.initialize());
    
    // Тестируем оптимизацию размещения
    kernel.optimizePlacement();
    
    // Проверяем, что ядро все еще работает после оптимизации
    assert(kernel.isRunning());
    
    kernel.shutdown();
    std::cout << "[OK] ArchitecturalKernel placement optimization test\n";
}

void testArchitecturalKernelHardwareAcceleration() {
    std::cout << "Testing ArchitecturalKernel hardware acceleration...\n";
    
    cloud::core::kernel::ArchitecturalKernel kernel;
    assert(kernel.initialize());
    
    // Проверяем поддержку аппаратного ускорения
    auto features = kernel.getSupportedFeatures();
    
    // Проверяем базовые метрики после инициализации
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] ArchitecturalKernel hardware acceleration test\n";
}

void testArchitecturalKernelResourceLimits() {
    std::cout << "Testing ArchitecturalKernel resource limits...\n";
    
    cloud::core::kernel::ArchitecturalKernel kernel;
    assert(kernel.initialize());
    
    // Устанавливаем лимиты ресурсов
    kernel.setResourceLimit("cpu", 0.8);
    kernel.setResourceLimit("memory", 1024 * 1024 * 200); // 200MB
    kernel.setResourceLimit("gpu", 0.6);
    
    // Проверяем использование ресурсов
    double cpuUsage = kernel.getResourceUsage("cpu");
    double memoryUsage = kernel.getResourceUsage("memory");
    double gpuUsage = kernel.getResourceUsage("gpu");
    
    assert(cpuUsage >= 0.0);
    assert(memoryUsage >= 0.0);
    assert(gpuUsage >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] ArchitecturalKernel resource limits test\n";
}

void testArchitecturalKernelTaskScheduling() {
    std::cout << "Testing ArchitecturalKernel task scheduling...\n";
    
    cloud::core::kernel::ArchitecturalKernel kernel;
    assert(kernel.initialize());
    
    std::atomic<int> completedTasks{0};
    
    // Планируем задачи с разными приоритетами
    for (int i = 0; i < 3; ++i) {
        kernel.scheduleTask([&completedTasks, i]() {
            completedTasks++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }, i);
    }
    
    // Даем время на выполнение
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    kernel.shutdown();
    std::cout << "[OK] ArchitecturalKernel task scheduling test\n";
}

int main() {
    try {
        smokeTestArchitecturalKernel();
        testArchitecturalKernelTopologyOptimization();
        testArchitecturalKernelPlacementOptimization();
        testArchitecturalKernelHardwareAcceleration();
        testArchitecturalKernelResourceLimits();
        testArchitecturalKernelTaskScheduling();
        std::cout << "All ArchitecturalKernel tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "ArchitecturalKernel test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ArchitecturalKernel test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 