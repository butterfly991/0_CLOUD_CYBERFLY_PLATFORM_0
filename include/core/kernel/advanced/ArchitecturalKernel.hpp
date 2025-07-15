#pragma once
#include <memory>
#include <string>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/CacheConfig.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"

namespace cloud {
namespace core {
namespace kernel {

// ArchitecturalKernel — ядро для оптимизации топологии, размещения задач и взаимодействия между ядрами.
// Использует ARM-ускорение, динамический кэш и PlatformOptimizer для адаптации под платформу (Apple Silicon, облако).
class ArchitecturalKernel : public IKernel {
public:
    ArchitecturalKernel();
    ~ArchitecturalKernel() override;

    // Инициализация ядра (создание всех компонентов, подготовка к работе).
    bool initialize() override;
    // Завершение работы ядра, освобождение ресурсов.
    void shutdown() override;

    // Оптимизация топологии вычислений (распределение задач между ядрами).
    void optimizeTopology();
    // Оптимизация размещения данных и задач (адаптация под платформу).
    void optimizePlacement();

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
    void scheduleTask(std::function<void()> task, int priority) override;

private:
    std::unique_ptr<core::drivers::ARMDriver> hardwareAccelerator; // ARM-ускорение (NEON/AMX)
    std::unique_ptr<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache; // Динамический кэш для архитектурных оптимизаций
    cloud::core::cache::PlatformOptimizer platformOptimizer; // Оптимизация под платформу
};

} // namespace kernel
} // namespace core
} // namespace cloud
