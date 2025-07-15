#pragma once
#include <memory>
#include <string>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/CacheConfig.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"

namespace cloud {
namespace core {
namespace kernel {

// OrchestrationKernel — ядро-оркестратор для управления очередями задач и их распределения между ядрами.
// Использует гибридную балансировку, динамический кэш, thread pool и recovery. Оптимизировано для облачных и ARM/Apple платформ.
class OrchestrationKernel : public IKernel {
public:
    OrchestrationKernel();
    ~OrchestrationKernel() override;

    // Инициализация ядра (создание всех компонентов, подготовка к работе).
    bool initialize() override;
    // Завершение работы ядра, освобождение ресурсов.
    void shutdown() override;

    // Добавление задачи в очередь (с приоритетом).
    void enqueueTask(const std::vector<uint8_t>& data, int priority = 5);

    // Балансировка задач между ядрами (интеграция с LoadBalancer).
    void balanceTasks();
    // Ускорение туннелей (пример интеграции с thread pool).
    void accelerateTunnels();

    // Оркестрация задач с учетом приоритета и метрик (интеграция с балансировщиком).
    void orchestrate(const std::vector<std::shared_ptr<IKernel>>& kernels);

    // Обновление метрик ядра (CPU, память, эффективность).
    void updateMetrics() override;

    // Методы IKernel (мониторинг, управление ресурсами, идентификация).
    bool isRunning() const override;
    metrics::PerformanceMetrics getMetrics() const override;
    void setResourceLimit(const std::string& resource, double limit) override;
    double getResourceUsage(const std::string& resource) const override;
    KernelType getType() const override;
    std::string getId() const override;
    void pause() override;
    void resume() override;
    void reset() override;
    std::vector<std::string> getSupportedFeatures() const override;
    void scheduleTask(std::function<void()> task, int priority) override {
        // Для тестов или расширения
    }

private:
    std::unique_ptr<cloud::core::balancer::LoadBalancer> loadBalancer; // Гибридная балансировка
    std::vector<std::vector<uint8_t>> taskQueue; // Очередь задач
    std::vector<balancer::TaskDescriptor> taskDescriptors; // Описания задач
    std::unique_ptr<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache; // Динамический кэш для задач
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool; // Пул потоков для оркестрации
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager; // Recovery для устойчивости
    cloud::core::cache::PlatformOptimizer platformOptimizer; // Оптимизация под платформу
    std::vector<balancer::KernelMetrics> getKernelMetrics(const std::vector<std::shared_ptr<IKernel>>& kernels) const;
};

} // namespace kernel
} // namespace core
} // namespace cloud
