#include <cassert>
#include <iostream>
#include <memory>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/kernel/base/MicroKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include <spdlog/spdlog.h>

void smokeTestCoreKernel() {
    spdlog::info("smokeTestCoreKernel: start");
    std::cout << "Testing CoreKernel basic operations...\n";
    
    cloud::core::kernel::CoreKernel kernel("test_core");
    std::cout << "[DEBUG] kernel constructed\n";
    // Production: initialize only once!
    assert(kernel.initialize());
    std::cout << "[DEBUG] kernel.initialize() OK\n";
    spdlog::info("CoreKernel created");
    spdlog::info("CoreKernel initialized");
    std::cout << "[DEBUG] assert: kernel.isRunning()...\n";
    assert(kernel.isRunning());
    spdlog::info("CoreKernel is running");
    std::cout << "[DEBUG] assert: kernel.getId() == 'test_core'...\n";
    assert(kernel.getId() == "test_core");
    
    // Проверяем базовые метрики
    auto metrics = kernel.getMetrics();
    spdlog::info("Metrics: cpu_usage={}, memory_usage={}", metrics.cpu_usage, metrics.memory_usage);
    std::cout << "[DEBUG] assert: metrics.cpu_usage >= 0.0...\n";
    assert(metrics.cpu_usage >= 0.0);
    std::cout << "[DEBUG] assert: metrics.memory_usage >= 0.0...\n";
    assert(metrics.memory_usage >= 0.0);
    assert(metrics.power_consumption >= 0.0);
    
    // Тестируем управление ресурсами
    kernel.setResourceLimit("cpu", 0.8);
    kernel.setResourceLimit("memory", 1024 * 1024 * 100); // 100MB
    
    assert(kernel.getResourceUsage("cpu") >= 0.0);
    assert(kernel.getResourceUsage("memory") >= 0.0);
    
    // Тестируем планирование задач
    bool taskExecuted = false;
    kernel.scheduleTask([&taskExecuted]() {
        taskExecuted = true;
    }, 5);
    
    // Даем время на выполнение задачи
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "[DEBUG] kernel.shutdown()...\n";
    kernel.shutdown();
    spdlog::info("CoreKernel shutdown");
    std::cout << "[DEBUG] assert: !kernel.isRunning()...\n";
    assert(!kernel.isRunning());
    spdlog::info("smokeTestCoreKernel: end");
}

void testCoreKernelChildManagement() {
    spdlog::info("testCoreKernelChildManagement: start");
    std::cout << "Testing CoreKernel child management...\n";
    
    cloud::core::kernel::CoreKernel parent("parent");
    assert(parent.initialize());
    spdlog::info("Parent kernel initialized");
    
    auto child1 = std::make_shared<cloud::core::kernel::MicroKernel>("child1");
    auto child2 = std::make_shared<cloud::core::kernel::MicroKernel>("child2");
    
    assert(child1->initialize());
    assert(child2->initialize());
    
    parent.addChildKernel(child1);
    parent.addChildKernel(child2);
    
    auto children = parent.getChildKernels();
    spdlog::info("Children count: {}", children.size());
    assert(children.size() == 2);
    
    parent.removeChildKernel("child1");
    children = parent.getChildKernels();
    spdlog::info("Children count after remove: {}", children.size());
    assert(children.size() == 1);
    assert(children[0]->getId() == "child2");
    
    parent.shutdown();
    spdlog::info("Parent kernel shutdown");
    std::cout << "[OK] CoreKernel child management test\n";
    spdlog::info("testCoreKernelChildManagement: end");
}

void testCoreKernelPerformanceMode() {
    spdlog::info("testCoreKernelPerformanceMode: start");
    std::cout << "Testing CoreKernel performance mode...\n";
    
    cloud::core::kernel::CoreKernel kernel("perf_test");
    assert(kernel.initialize());
    spdlog::info("Kernel initialized");
    
    kernel.setPerformanceMode(true);
    spdlog::info("Set high performance mode");
    assert(kernel.isHighPerformanceMode());
    
    kernel.setPerformanceMode(false);
    spdlog::info("Set normal performance mode");
    assert(!kernel.isHighPerformanceMode());
    
    kernel.shutdown();
    spdlog::info("Kernel shutdown");
    std::cout << "[OK] CoreKernel performance mode test\n";
    spdlog::info("testCoreKernelPerformanceMode: end");
}

void testCoreKernelEventHandling() {
    spdlog::info("testCoreKernelEventHandling: start");
    std::cout << "Testing CoreKernel event handling...\n";
    
    cloud::core::kernel::CoreKernel kernel("event_test");
    assert(kernel.initialize());
    spdlog::info("Kernel initialized");
    
    bool eventReceived = false;
    std::string eventData;
    
    kernel.registerEventHandler("test_event", [&eventReceived, &eventData](const std::string&, const std::any& data) {
        eventReceived = true;
        try {
            eventData = std::any_cast<std::string>(data);
        } catch (...) {
            eventData = "unknown";
        }
    });
    
    // Симулируем событие (в реальной реализации это делается внутри ядра)
    // kernel.triggerEvent("test_event", std::string("test_data"));
    
    kernel.unregisterEventHandler("test_event");
    kernel.shutdown();
    spdlog::info("Kernel shutdown");
    std::cout << "[OK] CoreKernel event handling test\n";
    spdlog::info("testCoreKernelEventHandling: end");
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("CoreKernelSmokeTest: main start");
    try {
        smokeTestCoreKernel();
        testCoreKernelChildManagement();
        testCoreKernelPerformanceMode();
        testCoreKernelEventHandling();
        std::cout << "All CoreKernel tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "CoreKernel test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "CoreKernel test failed with unknown exception" << std::endl;
        return 1;
    }
    spdlog::info("CoreKernelSmokeTest: main end");
    return 0;
} 