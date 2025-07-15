#include "core/kernel/advanced/ComputationalKernel.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/drivers/ARMDriver.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <atomic>

namespace cloud {
namespace core {
namespace kernel {

static std::atomic<bool> initialized{false};

ComputationalKernel::ComputationalKernel() {
    // Инициализация компонентов
    platformOptimizer = core::cache::PlatformOptimizer();
    
    // Создание ThreadPool для вычислений
    cloud::core::thread::ThreadPoolConfig threadConfig;
    threadConfig.minThreads = 2;
    threadConfig.maxThreads = 16;
    threadConfig.queueSize = 1024;
    threadConfig.stackSize = 1024 * 1024;
    threadPool = std::make_shared<core::thread::ThreadPool>(threadConfig);
    
    // Создание RecoveryManager
    core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 5;
    recoveryConfig.checkpointInterval = std::chrono::seconds(60);
    recoveryConfig.enableAutoRecovery = true;
    recoveryConfig.enableStateValidation = true;
    recoveryConfig.pointConfig.maxSize = 1024 * 1024 * 4; // 4MB
    recoveryConfig.pointConfig.enableCompression = true;
    recoveryConfig.pointConfig.storagePath = "./recovery_points/compute";
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::hours(12);
    recoveryConfig.logPath = "./logs/compute_recovery.log";
    recoveryConfig.maxLogSize = 1024 * 1024 * 2; // 2MB
    recoveryConfig.maxLogFiles = 2;
    
    recoveryManager = std::make_unique<core::recovery::RecoveryManager>(recoveryConfig);
    
    // Создание DynamicCache для вычислительных результатов
    auto cacheConfig = platformOptimizer.getOptimalConfig();
    dynamicCache = std::make_unique<core::cache::DynamicCache<std::string, std::vector<uint8_t>>>(
        cacheConfig.maxEntries / 2, 1800 // 30 минут TTL для вычислительных данных
    );
    
    // Создание ARM драйвера для аппаратного ускорения
    hardwareAccelerator = std::make_unique<core::drivers::ARMDriver>();
    
    spdlog::info("ComputationalKernel: создан");
}

ComputationalKernel::~ComputationalKernel() {
    shutdown();
}

bool ComputationalKernel::initialize() {
    auto start = std::chrono::high_resolution_clock::now();
    try {
        spdlog::info("ComputationalKernel: начало инициализации");
        
        // Инициализация ARM драйвера
        auto armStart = std::chrono::high_resolution_clock::now();
        if (!hardwareAccelerator->initialize()) {
            spdlog::warn("ComputationalKernel: ARM драйвер недоступен, используем программную реализацию");
        } else {
            auto armEnd = std::chrono::high_resolution_clock::now();
            auto armDuration = std::chrono::duration_cast<std::chrono::microseconds>(armEnd - armStart).count();
            spdlog::info("ComputationalKernel: ARM драйвер инициализирован за {} μs: {}", 
                        armDuration, hardwareAccelerator->getPlatformInfo());
        }
        
        // Инициализация RecoveryManager
        auto recoveryStart = std::chrono::high_resolution_clock::now();
        if (!recoveryManager->initialize()) {
            spdlog::error("ComputationalKernel: ошибка инициализации RecoveryManager");
            return false;
        }
        auto recoveryEnd = std::chrono::high_resolution_clock::now();
        auto recoveryDuration = std::chrono::duration_cast<std::chrono::microseconds>(recoveryEnd - recoveryStart).count();
        spdlog::info("ComputationalKernel: RecoveryManager инициализирован за {} μs", recoveryDuration);
        
        // Настройка DynamicCache
        auto cacheStart = std::chrono::high_resolution_clock::now();
        dynamicCache->setEvictionCallback([](const std::string& key, const std::vector<uint8_t>& data) {
            spdlog::debug("ComputationalKernel: промежуточный результат вытеснен: key={}, size={}", key, data.size());
        });
        
        dynamicCache->setAutoResize(true, 50, 5000);
        dynamicCache->setCleanupInterval(600); // 10 минут
        auto cacheEnd = std::chrono::high_resolution_clock::now();
        auto cacheDuration = std::chrono::duration_cast<std::chrono::microseconds>(cacheEnd - cacheStart).count();
        spdlog::info("ComputationalKernel: DynamicCache настроен за {} μs", cacheDuration);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        spdlog::info("ComputationalKernel: инициализация завершена успешно за {} μs", totalDuration);
        initialized.store(true);
        return true;
        
    } catch (const std::exception& e) {
        auto end = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        spdlog::error("ComputationalKernel: ошибка инициализации за {} μs: {}", totalDuration, e.what());
        initialized.store(false);
        return false;
    }
}

void ComputationalKernel::shutdown() {
    spdlog::info("ComputationalKernel: shutdown() start");
    bool error = false;
    std::string errorMsg;
    try {
        // dynamicCache
        if (dynamicCache) {
            try { dynamicCache->clear(); dynamicCache.reset(); }
            catch (const std::exception& e) { spdlog::error("ComputationalKernel: dynamicCache exception: {}", e.what()); error = true; errorMsg = "dynamicCache: " + std::string(e.what()); }
        }
        // recoveryManager
        if (recoveryManager) {
            try { recoveryManager->shutdown(); recoveryManager.reset(); }
            catch (const std::exception& e) { spdlog::error("ComputationalKernel: recoveryManager exception: {}", e.what()); error = true; errorMsg = "recoveryManager: " + std::string(e.what()); }
        }
        // threadPool
        if (threadPool) {
            try { threadPool->waitForCompletion(); threadPool.reset(); }
            catch (const std::exception& e) { spdlog::error("ComputationalKernel: threadPool exception: {}", e.what()); error = true; errorMsg = "threadPool: " + std::string(e.what()); }
        }
        // hardwareAccelerator
        if (hardwareAccelerator) {
            try { hardwareAccelerator->shutdown(); hardwareAccelerator.reset(); }
            catch (const std::exception& e) { spdlog::error("ComputationalKernel: hardwareAccelerator exception: {}", e.what()); error = true; errorMsg = "hardwareAccelerator: " + std::string(e.what()); }
        }
        // platformOptimizer
        try { /* platformOptimizer - не unique_ptr, просто reset если есть */ } catch (const std::exception& e) { spdlog::error("ComputationalKernel: platformOptimizer exception: {}", e.what()); error = true; errorMsg = "platformOptimizer: " + std::string(e.what()); }
    } catch (const std::exception& e) {
        spdlog::error("ComputationalKernel: shutdown() outer exception: {}", e.what());
        error = true;
        errorMsg = "outer: " + std::string(e.what());
    }
    initialized.store(false);
    spdlog::info("ComputationalKernel: shutdown() завершён (error: {})", error ? errorMsg : "none");
}

bool ComputationalKernel::compute(const std::vector<uint8_t>& data) {
    auto start = std::chrono::high_resolution_clock::now();
    try {
        spdlog::debug("ComputationalKernel: начало вычислений, размер данных: {}", data.size());
        
        // Создаем ключ для кэширования результата
        std::string resultKey = "compute_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        
        // Проверяем кэш на наличие готового результата
        auto cacheStart = std::chrono::high_resolution_clock::now();
        auto cachedResult = dynamicCache->get(resultKey);
        auto cacheEnd = std::chrono::high_resolution_clock::now();
        auto cacheDuration = std::chrono::duration_cast<std::chrono::microseconds>(cacheEnd - cacheStart).count();
        
        if (cachedResult) {
            auto end = std::chrono::high_resolution_clock::now();
            auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            spdlog::info("ComputationalKernel: результат найден в кэше за {} μs (поиск: {} μs)", 
                        totalDuration, cacheDuration);
            return true;
        }
        
        // Выполняем вычисления
    std::vector<uint8_t> result;
        auto computeStart = std::chrono::high_resolution_clock::now();
        
        if (hardwareAccelerator && hardwareAccelerator->isNeonSupported()) {
            // Используем аппаратное ускорение
            if (hardwareAccelerator->accelerateCopy(data, result)) {
                auto computeEnd = std::chrono::high_resolution_clock::now();
                auto computeDuration = std::chrono::duration_cast<std::chrono::microseconds>(computeEnd - computeStart).count();
                spdlog::info("ComputationalKernel: вычисления выполнены с аппаратным ускорением за {} μs", computeDuration);
            } else {
                // Fallback на программную реализацию
                result = performSoftwareComputation(data);
                auto computeEnd = std::chrono::high_resolution_clock::now();
                auto computeDuration = std::chrono::duration_cast<std::chrono::microseconds>(computeEnd - computeStart).count();
                spdlog::info("ComputationalKernel: вычисления выполнены программно за {} μs", computeDuration);
            }
        } else {
            // Программная реализация
            result = performSoftwareComputation(data);
            auto computeEnd = std::chrono::high_resolution_clock::now();
            auto computeDuration = std::chrono::duration_cast<std::chrono::microseconds>(computeEnd - computeStart).count();
            spdlog::info("ComputationalKernel: вычисления выполнены программно за {} μs", computeDuration);
        }
        
        // Сохраняем результат в кэш
        auto saveStart = std::chrono::high_resolution_clock::now();
        dynamicCache->put(resultKey, result);
        auto saveEnd = std::chrono::high_resolution_clock::now();
        auto saveDuration = std::chrono::duration_cast<std::chrono::microseconds>(saveEnd - saveStart).count();
        spdlog::debug("ComputationalKernel: результат сохранен в кэш за {} μs", saveDuration);
        
        // Создаем точку восстановления
        if (recoveryManager) {
            auto recoveryStart = std::chrono::high_resolution_clock::now();
            std::string pointId = recoveryManager->createRecoveryPoint();
            auto recoveryEnd = std::chrono::high_resolution_clock::now();
            auto recoveryDuration = std::chrono::duration_cast<std::chrono::microseconds>(recoveryEnd - recoveryStart).count();
            spdlog::trace("ComputationalKernel: создана точка восстановления '{}' за {} μs", pointId, recoveryDuration);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        spdlog::info("ComputationalKernel: вычисления завершены успешно за {} μs (кэш: {} μs, вычисления: {} μs, сохранение: {} μs)", 
                    totalDuration, cacheDuration, 
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - computeStart).count(),
                    saveDuration);
        return true;
        
    } catch (const std::exception& e) {
        auto end = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        spdlog::error("ComputationalKernel: ошибка вычислений за {} μs: {}", totalDuration, e.what());
        return false;
    }
}

void ComputationalKernel::updateMetrics() {
    try {
        // Обновляем метрики всех компонентов
        if (threadPool) {
            threadPool->updateMetrics();
        }
        
        // Обновляем метрики кэша
        if (dynamicCache) {
            // DynamicCache не имеет updateMetrics
        }
        
        spdlog::trace("ComputationalKernel: метрики обновлены");
        
    } catch (const std::exception& e) {
        spdlog::error("ComputationalKernel: ошибка обновления метрик: {}", e.what());
    }
}

bool ComputationalKernel::isRunning() const {
    return initialized.load();
}

metrics::PerformanceMetrics ComputationalKernel::getMetrics() const {
    metrics::PerformanceMetrics metrics{};
    
    try {
        if (threadPool) {
            auto threadMetrics = threadPool->getMetrics();
            metrics.cpu_usage = static_cast<double>(threadMetrics.activeThreads) / threadMetrics.totalThreads;
        }
        
        if (dynamicCache) {
            metrics.memory_usage = static_cast<double>(dynamicCache->size()) / 1000.0;
        }
        
        // Добавляем специфичные для вычислений метрики
        if (hardwareAccelerator) {
            metrics.efficiencyScore = hardwareAccelerator->isNeonSupported() ? 0.9 : 0.7;
        } else {
            metrics.efficiencyScore = 0.6;
        }
        
        metrics.timestamp = std::chrono::steady_clock::now();
        
    } catch (const std::exception& e) {
        spdlog::error("ComputationalKernel: ошибка получения метрик: {}", e.what());
    }
    
    return metrics;
}

void ComputationalKernel::setResourceLimit(const std::string& resource, double limit) {
    try {
        if (resource == "threads" && threadPool) {
            cloud::core::thread::ThreadPoolConfig config = threadPool->getConfiguration();
            config.maxThreads = static_cast<size_t>(limit);
            threadPool->setConfiguration(config);
            spdlog::info("ComputationalKernel: установлен лимит потоков {}", limit);
        } else if (resource == "cache" && dynamicCache) {
            dynamicCache->resize(static_cast<size_t>(limit));
            spdlog::info("ComputationalKernel: установлен лимит кэша {}", limit);
        } else {
            spdlog::warn("ComputationalKernel: неизвестный ресурс '{}'", resource);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("ComputationalKernel: ошибка установки лимита ресурса: {}", e.what());
    }
}

double ComputationalKernel::getResourceUsage(const std::string& resource) const {
    try {
        if (resource == "threads" && threadPool) {
            auto metrics = threadPool->getMetrics();
            return static_cast<double>(metrics.activeThreads);
        } else if (resource == "cache" && dynamicCache) {
            return static_cast<double>(dynamicCache->size());
        } else {
            spdlog::warn("ComputationalKernel: неизвестный ресурс '{}'", resource);
            return 0.0;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("ComputationalKernel: ошибка получения использования ресурса: {}", e.what());
        return 0.0;
    }
}

KernelType ComputationalKernel::getType() const {
    return KernelType::COMPUTATIONAL;
}

std::string ComputationalKernel::getId() const {
    return "computational_kernel";
}

void ComputationalKernel::pause() {
    spdlog::info("ComputationalKernel: приостановлен");
}

void ComputationalKernel::resume() {
    spdlog::info("ComputationalKernel: возобновлен");
}

void ComputationalKernel::reset() {
    try {
        if (dynamicCache) {
            dynamicCache->clear();
        }
        
        spdlog::info("ComputationalKernel: сброшен");
        
    } catch (const std::exception& e) {
        spdlog::error("ComputationalKernel: ошибка сброса: {}", e.what());
    }
}

std::vector<std::string> ComputationalKernel::getSupportedFeatures() const {
    std::vector<std::string> features = {
        "hardware_acceleration",
        "neon_optimization",
        "cache_optimization",
        "recovery_management",
        "dynamic_thread_pool"
    };
    
    if (hardwareAccelerator) {
        if (hardwareAccelerator->isNeonSupported()) {
            features.push_back("neon_support");
        }
        if (hardwareAccelerator->isAMXSupported()) {
            features.push_back("amx_support");
        }
    }
    
    return features;
}

void ComputationalKernel::scheduleTask(std::function<void()> task, int priority) {
    if (threadPool) {
        threadPool->enqueue(std::move(task));
    }
}

// Приватные методы

std::vector<uint8_t> ComputationalKernel::performSoftwareComputation(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result;
    result.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        uint8_t processed = data[i];
        processed = (processed * 5 + 11) % 256;
        processed = processed ^ 0x3C;
        processed = (processed + 23) % 256;
        result.push_back(processed);
    }
    return result;
}

} // namespace kernel
} // namespace core
} // namespace cloud
