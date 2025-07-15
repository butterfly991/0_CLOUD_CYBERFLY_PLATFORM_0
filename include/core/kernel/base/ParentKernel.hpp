#pragma once
#include <vector>
#include <memory>
#include <string>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/balancer/EnergyController.hpp"
#include "core/kernel/advanced/OrchestrationKernel.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"

namespace cloud {
namespace core {
namespace kernel {

// ParentKernel — управление группой ядер, балансировка, энергоменеджмент, recovery, thread pool, оптимизация
class ParentKernel : public IKernel {
public:
    ParentKernel(); // Конструктор
    ~ParentKernel(); // Деструктор
    bool initialize() override; // Инициализация
    void shutdown() override; // Завершение работы
    void addChild(std::shared_ptr<IKernel> child); // Добавить дочернее ядро
    void removeChild(const std::string& id); // Удалить дочернее ядро
    void balanceLoad(); // Балансировка нагрузки
    void orchestrateTasks(); // Оркестрация задач
    void updateMetrics() override; // Обновить метрики
    std::vector<std::shared_ptr<IKernel>> getChildren() const; // Дочерние ядра
    bool isRunning() const override; // Статус
    metrics::PerformanceMetrics getMetrics() const override; // Метрики
    void setResourceLimit(const std::string& resource, double limit) override; // Лимит ресурса
    double getResourceUsage(const std::string& resource) const override; // Использование ресурса
    KernelType getType() const override; // Тип ядра
    std::string getId() const override; // ID ядра
    void pause() override; // Пауза
    void resume() override; // Возобновление
    void reset() override; // Сброс
    std::vector<std::string> getSupportedFeatures() const override; // Фичи
    void scheduleTask(std::function<void()> task, int priority) override; // Планирование задачи
    void setLoadBalancer(std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer); // Балансировщик
    std::shared_ptr<cloud::core::balancer::LoadBalancer> getLoadBalancer() const; // Балансировщик
private:
    std::vector<std::shared_ptr<IKernel>> children; // Дочерние ядра
    std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer; // Балансировщик нагрузки
    std::unique_ptr<cloud::core::balancer::EnergyController> energyController; // Энергоконтроллер
    std::unique_ptr<cloud::core::kernel::OrchestrationKernel> orchestrationKernel; // Оркестратор задач
    std::unique_ptr<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache; // Динамический кэш
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool; // Пул потоков
    std::unique_ptr<cloud::core::recovery::RecoveryManager> recoveryManager; // Recovery
    std::unique_ptr<cloud::core::cache::PlatformOptimizer> platformOptimizer; // Оптимизация под платформу
};

} // namespace kernel
} // namespace core
} // namespace cloud
