#pragma once
#include <memory>
#include <string>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/CacheConfig.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/drivers/ARMDriver.hpp"

namespace cloud {
namespace core {
namespace kernel {

// ComputationalKernel — ядро для вычислительных задач, использует ARM-ускорение, динамический кэш и thread pool.
// Оптимизировано для Apple Silicon, облачных ARM и x86-64 платформ. Интеграция с PlatformOptimizer для адаптации под архитектуру.
class ComputationalKernel : public IKernel {
public:
    ComputationalKernel();
    ~ComputationalKernel() override;
    // Инициализация ядра (создание всех компонентов, подготовка к работе).
    bool initialize() override;
    // Завершение работы ядра, освобождение ресурсов.
    void shutdown() override;
    // Выполнение вычислительной задачи (с возможностью аппаратного ускорения).
    bool compute(const std::vector<uint8_t>& data);
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
    // Программная реализация вычислений (fallback, если нет аппаратного ускорения).
    static std::vector<uint8_t> performSoftwareComputation(const std::vector<uint8_t>& data);
private:
    std::unique_ptr<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache; // Динамический кэш для результатов
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool; // Пул потоков для вычислений
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager; // Recovery для устойчивости
    cloud::core::cache::PlatformOptimizer platformOptimizer; // Оптимизация под платформу
    std::unique_ptr<core::drivers::ARMDriver> hardwareAccelerator; // ARM-ускорение (NEON/AMX)
};

} // namespace kernel
} // namespace core
} // namespace cloud
