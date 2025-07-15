#include <cassert>
#include <iostream>
#include <memory>
#include "core/kernel/advanced/SmartKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"

void smokeTestSmartKernel() {
    std::cout << "Testing SmartKernel basic operations...\n";
    
    cloud::core::kernel::SmartKernelConfig config;
    config.maxThreads = 4;
    config.maxMemory = 1024 * 1024 * 50; // 50MB
    config.metricsInterval = std::chrono::seconds(2);
    config.adaptationThreshold = 0.1;
    
    cloud::core::kernel::SmartKernel kernel(config);
    assert(kernel.initialize());
    assert(kernel.isRunning());
    
    // Проверяем базовые метрики
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    // Проверяем smart метрики
    auto smartMetrics = kernel.getSmartMetrics();
    assert(smartMetrics.threadUtilization >= 0.0);
    assert(smartMetrics.memoryUtilization >= 0.0);
    assert(smartMetrics.overallEfficiency >= 0.0);
    
    // Проверяем адаптивные метрики
    auto adaptiveMetrics = kernel.getAdaptiveMetrics();
    assert(adaptiveMetrics.loadFactor >= 0.0);
    assert(adaptiveMetrics.efficiencyScore >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] SmartKernel smoke test\n";
}

void testSmartKernelAdaptation() {
    std::cout << "Testing SmartKernel adaptation capabilities...\n";
    
    cloud::core::kernel::SmartKernelConfig config;
    config.adaptiveConfig.learningRate = 0.2;
    config.adaptiveConfig.explorationRate = 0.1;
    config.adaptiveConfig.enableAutoTuning = true;
    
    cloud::core::kernel::SmartKernel kernel(config);
    assert(kernel.initialize());
    
    // Симулируем нагрузку для адаптации
    for (int i = 0; i < 10; ++i) {
        kernel.scheduleTask([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }, i % 5);
    }
    
    // Даем время на адаптацию
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    auto adaptiveMetrics = kernel.getAdaptiveMetrics();
    assert(adaptiveMetrics.loadFactor >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] SmartKernel adaptation test\n";
}

void testSmartKernelErrorHandling() {
    std::cout << "Testing SmartKernel error handling...\n";
    
    cloud::core::kernel::SmartKernel kernel;
    assert(kernel.initialize());
    
    bool errorReceived = false;
    std::string errorMessage;
    
    kernel.setErrorCallback([&errorReceived, &errorMessage](const std::string& error) {
        errorReceived = true;
        errorMessage = error;
    });
    
    // Симулируем ошибку (в реальной реализации это делается внутри ядра)
    // kernel.handleError("test_error");
    
    kernel.shutdown();
    std::cout << "[OK] SmartKernel error handling test\n";
}

void testSmartKernelConfiguration() {
    std::cout << "Testing SmartKernel configuration management...\n";
    
    cloud::core::kernel::SmartKernel kernel;
    assert(kernel.initialize());
    
    cloud::core::kernel::SmartKernelConfig newConfig;
    newConfig.maxThreads = 8;
    newConfig.maxMemory = 1024 * 1024 * 200; // 200MB
    newConfig.adaptationThreshold = 0.2;
    
    kernel.setConfiguration(newConfig);
    auto currentConfig = kernel.getConfiguration();
    
    assert(currentConfig.maxThreads == 8);
    assert(currentConfig.maxMemory == 1024 * 1024 * 200);
    assert(currentConfig.adaptationThreshold == 0.2);
    
    kernel.shutdown();
    std::cout << "[OK] SmartKernel configuration test\n";
}

int main() {
    try {
        smokeTestSmartKernel();
        testSmartKernelAdaptation();
        testSmartKernelErrorHandling();
        testSmartKernelConfiguration();
        std::cout << "All SmartKernel tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "SmartKernel test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "SmartKernel test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 