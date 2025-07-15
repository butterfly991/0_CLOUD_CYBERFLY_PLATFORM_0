#pragma once
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <unordered_map>
#include <deque>
#include <random>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "core/kernel/base/CoreKernel.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/CacheConfig.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/LoadBalancer.hpp"

namespace cloud {
namespace core {
namespace kernel {

namespace detail {
    class PerformanceMonitor;
    class ResourceManager;
    class AdaptiveController;
}

namespace config {
    struct AdaptiveConfig {
        double learningRate = 0.1;
        double explorationRate = 0.1;
        size_t historySize = 10;
        std::chrono::milliseconds adaptationInterval{1000};
        bool enableAutoTuning = true;
        
        bool validate() const {
            return learningRate > 0.0 && learningRate <= 1.0 &&
                   explorationRate >= 0.0 && explorationRate <= 1.0 &&
                   historySize > 0 && adaptationInterval.count() > 0;
        }
    };
    
    struct ResourceConfig {
        size_t minThreads = 2;
        size_t maxThreads = 16;
        size_t cacheSize = 1024 * 1024; // 1MB
        size_t memoryLimit = 1024 * 1024 * 100; // 100MB
        double cpuLimit = 0.8;
        
        bool validate() const {
            return minThreads > 0 && maxThreads >= minThreads &&
                   cacheSize > 0 && memoryLimit > 0 &&
                   cpuLimit > 0.0 && cpuLimit <= 1.0;
        }
    };
}

namespace metrics {
    struct AdaptiveMetrics {
        double loadFactor = 0.0;
        double efficiencyScore = 0.0;
        double powerEfficiency = 0.0;
        double thermalEfficiency = 0.0;
        double resourceUtilization = 0.0;
        std::chrono::steady_clock::time_point lastAdaptation = std::chrono::steady_clock::now();
        
        nlohmann::json toJson() const {
            return {
                {"loadFactor", loadFactor},
                {"efficiencyScore", efficiencyScore},
                {"powerEfficiency", powerEfficiency},
                {"thermalEfficiency", thermalEfficiency},
                {"resourceUtilization", resourceUtilization},
                {"lastAdaptation", std::chrono::duration_cast<std::chrono::milliseconds>(
                    lastAdaptation.time_since_epoch()).count()}
            };
        }
    };
}

struct SmartKernelConfig {
    size_t maxThreads = 8;
    size_t maxMemory = 1024 * 1024 * 100; // 100MB
    std::chrono::seconds metricsInterval{5};
    double adaptationThreshold = 0.1;
    
    config::AdaptiveConfig adaptiveConfig;
    config::ResourceConfig resourceConfig;
    
    bool validate() const {
        return maxThreads > 0 && maxMemory > 0 && 
               metricsInterval.count() > 0 && adaptationThreshold > 0.0 &&
               adaptiveConfig.validate() && resourceConfig.validate();
    }
};

struct SmartKernelMetrics {
    double threadUtilization = 0.0;
    double memoryUtilization = 0.0;
    double cacheEfficiency = 0.0;
    double preloadEfficiency = 0.0;
    double recoveryEfficiency = 0.0;
    double overallEfficiency = 0.0;
};

// SmartKernel — адаптивное ядро с поддержкой автоматической настройки, thread pool, recovery и динамического кэша.
// Использует ARM/Apple оптимизации, PlatformOptimizer, гибридную балансировку и энергоменеджмент. Предназначено для облачных и высоконагруженных систем.
class SmartKernel : public CoreKernel {
public:
    explicit SmartKernel(const SmartKernelConfig& config = SmartKernelConfig{});
    ~SmartKernel() override;
    
    SmartKernel(const SmartKernel&) = delete;
    SmartKernel& operator=(const SmartKernel&) = delete;
    SmartKernel(SmartKernel&&) noexcept;
    SmartKernel& operator=(SmartKernel&&) noexcept;
    
    // Инициализация ядра (создание всех компонентов, подготовка к работе).
    bool initialize() override;
    // Завершение работы ядра, освобождение ресурсов.
    void shutdown() override;
    // Получение метрик ядра (CPU, память, эффективность, адаптивные показатели).
    metrics::PerformanceMetrics getMetrics() const override;
    // Обновление метрик ядра (адаптация thread pool, кэша, recovery).
    void updateMetrics() override;
    // Установка новой конфигурации ядра (адаптация под нагрузку).
    void setConfiguration(const SmartKernelConfig& config);
    // Получение текущей конфигурации ядра.
    SmartKernelConfig getConfiguration() const;
    // Получение smart-метрик (thread pool, кэш, recovery, эффективность).
    SmartKernelMetrics getSmartMetrics() const;
    // Получение адаптивных метрик (адаптация, эффективность, энергопотребление).
    metrics::AdaptiveMetrics getAdaptiveMetrics() const;
    // Установка обработчика ошибок (callback для критических ситуаций).
    void setErrorCallback(std::function<void(const std::string&)> callback);
    // Проверка, работает ли ядро (true — если инициализировано и не завершено).
    bool isRunning() const override;
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    std::shared_ptr<detail::PerformanceMonitor> performanceMonitor; // Мониторинг производительности
    std::shared_ptr<detail::ResourceManager> resourceManager; // Управление ресурсами
    std::shared_ptr<detail::AdaptiveController> adaptiveController; // Адаптация параметров
    std::shared_ptr<core::thread::ThreadPool> threadPool; // Пул потоков
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager; // Recovery для устойчивости
    std::unique_ptr<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache; // Динамический кэш
    cloud::core::cache::PlatformOptimizer platformOptimizer; // Оптимизация под платформу
    mutable std::shared_mutex kernelMutex;
    std::atomic<bool> initialized{false};
    std::atomic<bool> isOptimizing{false};
    void initializeLogger();
    void initializeComponents();
    void shutdownComponents();
    void handleError(const std::string& error);
    void adaptThreadPool(const metrics::AdaptiveMetrics& metrics);
    void adaptCacheSize(const metrics::AdaptiveMetrics& metrics);
    void adaptRecovery();
};

} // namespace kernel
} // namespace core
} // namespace cloud
