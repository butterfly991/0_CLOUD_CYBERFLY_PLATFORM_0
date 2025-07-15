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

// CryptoMicroKernel — ядро для криптографических операций, использует ARM-ускорение, динамический кэш и thread pool.
// Оптимизировано для Apple Silicon, облачных ARM и x86-64 платформ. Интеграция с PlatformOptimizer для адаптации под архитектуру.
class CryptoMicroKernel : public IKernel {
public:
    explicit CryptoMicroKernel(const std::string& id);
    ~CryptoMicroKernel() override;
    // Инициализация ядра (создание всех компонентов, подготовка к работе).
    bool initialize() override;
    // Завершение работы ядра, освобождение ресурсов.
    void shutdown() override;
    // Выполнение криптографической задачи (с возможностью аппаратного ускорения).
    bool executeCryptoTask(const std::vector<uint8_t>& data, std::vector<uint8_t>& result);
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
    // Аппаратно-ускоренная криптография (NEON/AMX, если доступно).
    bool performHardwareAcceleratedCrypto(const std::vector<uint8_t>& data, std::vector<uint8_t>& result);
    // Программная реализация криптографии (fallback).
    std::vector<uint8_t> performSoftwareCrypto(const std::vector<uint8_t>& data);
private:
    std::string id;
    std::unique_ptr<core::drivers::ARMDriver> armDriver; // ARM-ускорение (NEON/AMX)
    std::unique_ptr<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache; // Динамический кэш для результатов
    std::shared_ptr<core::thread::ThreadPool> threadPool; // Пул потоков для криптографии
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager; // Recovery для устойчивости
    cloud::core::cache::PlatformOptimizer platformOptimizer; // Оптимизация под платформу
};

} // namespace kernel
} // namespace core
} // namespace cloud 