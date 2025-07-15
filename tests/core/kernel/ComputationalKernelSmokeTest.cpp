#include <cassert>
#include <iostream>
#include <memory>
#include "core/kernel/advanced/ComputationalKernel.hpp"

void smokeTestComputationalKernel() {
    std::cout << "Testing ComputationalKernel basic operations...\n";
    
    cloud::core::kernel::ComputationalKernel kernel;
    assert(kernel.initialize());
    assert(kernel.isRunning());
    assert(kernel.getType() == cloud::core::kernel::KernelType::COMPUTATIONAL);
    
    // Проверяем базовые метрики
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    kernel.shutdown();
    assert(!kernel.isRunning());
    
    std::cout << "[OK] ComputationalKernel smoke test\n";
}

void testComputationalKernelCompute() {
    std::cout << "Testing ComputationalKernel compute operations...\n";
    
    cloud::core::kernel::ComputationalKernel kernel;
    assert(kernel.initialize());
    
    // Тестовые данные
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    // Тестируем вычислительную операцию
    bool result = kernel.compute(testData);
    assert(result);
    
    kernel.shutdown();
    std::cout << "[OK] ComputationalKernel compute test\n";
}

void testComputationalKernelSoftwareComputation() {
    std::cout << "Testing ComputationalKernel software computation...\n";
    
    std::vector<uint8_t> inputData = {1, 2, 3, 4, 5};
    auto result = cloud::core::kernel::ComputationalKernel::performSoftwareComputation(inputData);
    
    // Проверяем, что результат не пустой
    assert(!result.empty());
    assert(result.size() > 0);
    
    std::cout << "[OK] ComputationalKernel software computation test\n";
}

void testComputationalKernelResourceManagement() {
    std::cout << "Testing ComputationalKernel resource management...\n";
    
    cloud::core::kernel::ComputationalKernel kernel;
    assert(kernel.initialize());
    
    // Устанавливаем лимиты ресурсов
    kernel.setResourceLimit("cpu", 0.9);
    kernel.setResourceLimit("memory", 1024 * 1024 * 100); // 100MB
    
    // Проверяем использование ресурсов
    double cpuUsage = kernel.getResourceUsage("cpu");
    double memoryUsage = kernel.getResourceUsage("memory");
    
    assert(cpuUsage >= 0.0);
    assert(memoryUsage >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] ComputationalKernel resource management test\n";
}

void testComputationalKernelTaskScheduling() {
    std::cout << "Testing ComputationalKernel task scheduling...\n";
    
    cloud::core::kernel::ComputationalKernel kernel;
    assert(kernel.initialize());
    
    std::atomic<int> taskCounter{0};
    
    // Планируем несколько задач
    for (int i = 0; i < 5; ++i) {
        kernel.scheduleTask([&taskCounter, i]() {
            taskCounter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }, i % 3);
    }
    
    // Даем время на выполнение задач
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    kernel.shutdown();
    std::cout << "[OK] ComputationalKernel task scheduling test\n";
}

int main() {
    try {
        smokeTestComputationalKernel();
        testComputationalKernelCompute();
        testComputationalKernelSoftwareComputation();
        testComputationalKernelResourceManagement();
        testComputationalKernelTaskScheduling();
        std::cout << "All ComputationalKernel tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "ComputationalKernel test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ComputationalKernel test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 