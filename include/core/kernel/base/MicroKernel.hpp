#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include "core/balancer/TaskTypes.hpp"
#include "core/kernel/base/CoreKernel.hpp"

namespace cloud {
namespace core {
namespace kernel {

// MicroKernel — минималистичное ядро для сервисных и криптозадач, ARM/Apple, кэш, thread pool, recovery, preload, балансировка
class MicroKernel : public IKernel {
public:
    explicit MicroKernel(const std::string& id); // Конструктор
    ~MicroKernel() override; // Деструктор
    bool initialize() override; // Инициализация
    void shutdown() override;   // Завершение работы
    virtual bool executeTask(const std::vector<uint8_t>& data); // Выполнение задачи
    void updateMetrics() override; // Обновить метрики
    bool isRunning() const override; // Статус ядра
    metrics::PerformanceMetrics getMetrics() const override; // Метрики
    void setResourceLimit(const std::string& resource, double limit) override; // Лимит ресурса
    double getResourceUsage(const std::string& resource) const override; // Использование ресурса
    KernelType getType() const override; // Тип ядра
    std::string getId() const override;  // ID ядра
    void pause() override;   // Пауза
    void resume() override;  // Возобновление
    void reset() override;   // Сброс
    std::vector<std::string> getSupportedFeatures() const override; // Фичи
    void setPreloadManager(std::shared_ptr<core::PreloadManager> preloadManager); // Preload
    void warmupFromPreload(); // Warm-up
    ExtendedKernelMetrics getExtendedMetrics() const; // Метрики для балансировщика
    void updateExtendedMetrics(); // Обновить расширенные метрики
    bool processTask(const cloud::core::balancer::TaskDescriptor& task); // Задача от балансировщика
    void scheduleTask(std::function<void()> task, int priority) override; // Планирование задачи
    void setTaskCallback(TaskCallback callback); // Callback задачи
    void setLoadBalancer(std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer); // Балансировщик
    std::shared_ptr<cloud::core::balancer::LoadBalancer> getLoadBalancer() const; // Балансировщик
    void setEventCallback(const std::string& event, EventCallback callback); // Callback события
    void removeEventCallback(const std::string& event); // Удалить callback
    void triggerEvent(const std::string& event, const std::any& data); // Вызвать событие
protected:
    std::string id; // ID
    std::unique_ptr<core::cache::DefaultDynamicCache> dynamicCache; // Кэш
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool; // Потоки
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager; // Recovery
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer; // Оптимизация
    std::shared_ptr<core::PreloadManager> preloadManager_; // Preload
    std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer_; // Балансировщик
    TaskCallback taskCallback_; // Callback задачи
    std::unordered_map<std::string, EventCallback> eventCallbacks_; // Callback-и событий
    ExtendedKernelMetrics extendedMetrics_; // Метрики для балансировщика
    mutable std::shared_mutex kernelMutex_; // Мьютекс
    std::atomic<bool> running_{false}; // Статус
    void initializePreloadManager();
    void initializeLoadBalancer();
    void updateExtendedMetricsFromPerformance();
    void notifyEvent(const std::string& event, const std::any& data);
};

} // namespace kernel
} // namespace core
} // namespace cloud
