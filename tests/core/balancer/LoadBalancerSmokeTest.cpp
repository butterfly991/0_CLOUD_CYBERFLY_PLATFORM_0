#include <cassert>
#include <iostream>
#include <memory>
#include "core/balancer/LoadBalancer.hpp"
#include "core/kernel/base/CoreKernel.hpp"
#include "core/balancer/TaskTypes.hpp"

class DummyKernel : public cloud::core::kernel::IKernel {
public:
    bool initialize() override { return true; }
    void shutdown() override {}
    bool isRunning() const override { return true; }
    cloud::core::kernel::metrics::PerformanceMetrics getMetrics() const override { return {0.5, 0.1, 0.0, 0.0, 0, std::chrono::steady_clock::now()}; }
    void updateMetrics() override {}
    void setResourceLimit(const std::string&, double) override {}
    double getResourceUsage(const std::string&) const override { return 0.5; }
    cloud::core::kernel::KernelType getType() const override { return cloud::core::kernel::KernelType::MICRO; }
    std::string getId() const override { return "dummy"; }
    void pause() override {}
    void resume() override {}
    void reset() override {}
    std::vector<std::string> getSupportedFeatures() const override { return {}; }
    void scheduleTask(std::function<void()> task, int priority) override {}
};

void smokeTestLoadBalancer() {
    cloud::core::balancer::LoadBalancer lb;
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels = {std::make_shared<DummyKernel>(), std::make_shared<DummyKernel>()};
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    
    // Создаем задачи разных типов
    cloud::core::balancer::TaskDescriptor t1;
    t1.data = {1,2,3};
    t1.priority = 10;
    t1.type = cloud::core::balancer::TaskType::CPU_INTENSIVE;
    t1.enqueueTime = std::chrono::steady_clock::now();
    tasks.push_back(t1);
    
    cloud::core::balancer::TaskDescriptor t2;
    t2.data = {4,5,6};
    t2.priority = 1;
    t2.type = cloud::core::balancer::TaskType::IO_INTENSIVE;
    t2.enqueueTime = std::chrono::steady_clock::now();
    tasks.push_back(t2);
    
    // Создаем разные метрики для ядер с разными эффективностями
    std::vector<cloud::core::balancer::KernelMetrics> metrics = {
        // Ядро 0: высокая загрузка CPU, но хорошая эффективность для CPU-задач
        {0.8, 0.3, 1000.0, 1000.0, 80.0, 0.9, 0.3, 0.5, 0.4},
        // Ядро 1: низкая загрузка CPU, хорошая эффективность для I/O-задач
        {0.2, 0.1, 800.0, 1000.0, 40.0, 0.4, 0.9, 0.6, 0.7}
    };
    
    lb.balance(kernels, tasks, metrics);
    std::cout << "[OK] LoadBalancer smoke test\n";
}

void stressTestLoadBalancer() {
    cloud::core::balancer::LoadBalancer lb;
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels;
    for (int i = 0; i < 32; ++i) kernels.push_back(std::make_shared<DummyKernel>());
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    for (int i = 0; i < 10000; ++i) {
        cloud::core::balancer::TaskDescriptor t;
        t.data = std::vector<uint8_t>(100, i % 256);
        t.priority = i % 10;
        t.type = static_cast<cloud::core::balancer::TaskType>(i % 5); // Разные типы задач
        t.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(t);
    }
    
    // Создаем разнообразные метрики для 32 ядер
    std::vector<cloud::core::balancer::KernelMetrics> metrics;
    for (int i = 0; i < 32; ++i) {
        cloud::core::balancer::KernelMetrics metric;
        metric.cpuUsage = 0.1 + (i % 10) * 0.08; // 0.1 - 0.82
        metric.memoryUsage = 0.05 + (i % 8) * 0.1; // 0.05 - 0.75
        metric.networkBandwidth = 500.0 + (i % 20) * 50.0; // 500 - 1450 MB/s
        metric.diskIO = 800.0 + (i % 15) * 100.0; // 800 - 2200 IOPS
        metric.energyConsumption = 30.0 + (i % 12) * 5.0; // 30 - 85 Watts
        metric.cpuTaskEfficiency = 0.3 + (i % 7) * 0.1; // 0.3 - 0.9
        metric.ioTaskEfficiency = 0.2 + (i % 9) * 0.08; // 0.2 - 0.92
        metric.memoryTaskEfficiency = 0.4 + (i % 6) * 0.1; // 0.4 - 0.9
        metric.networkTaskEfficiency = 0.25 + (i % 8) * 0.09; // 0.25 - 0.97
        metrics.push_back(metric);
    }
    
    lb.balance(kernels, tasks, metrics);
    std::cout << "[OK] LoadBalancer stress test\n";
}

int main() {
    smokeTestLoadBalancer();
    stressTestLoadBalancer();
    std::cout << "All LoadBalancer tests passed!\n";
    return 0;
} 