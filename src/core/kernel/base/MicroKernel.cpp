#include "core/kernel/base/MicroKernel.hpp"
#include <spdlog/spdlog.h>
#include "core/cache/base/AdaptiveCache.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

MicroKernel::MicroKernel(const std::string& id_) : id(id_) {
    platformOptimizer = std::make_unique<core::cache::PlatformOptimizer>();
    auto cacheConfig = platformOptimizer->getOptimalConfig();
    dynamicCache = std::make_unique<core::cache::DefaultDynamicCache>(cacheConfig.maxSize);
    
    // Создаем ThreadPool с базовой конфигурацией
    cloud::core::thread::ThreadPoolConfig threadConfig;
    threadConfig.minThreads = 2;
    threadConfig.maxThreads = 8;
    threadConfig.queueSize = 1024;
    threadConfig.stackSize = 1024 * 1024;
    threadPool = std::make_shared<cloud::core::thread::ThreadPool>(threadConfig);
    
    recoveryManager = std::make_unique<core::recovery::RecoveryManager>();
}

MicroKernel::~MicroKernel() = default;

bool MicroKernel::initialize() {
    spdlog::info("MicroKernel[{}]: инициализация", id);
    bool ok = true;
    
    try {
        // Инициализация интеграции
        initializePreloadManager();
        initializeLoadBalancer();
        
        // Инициализируем RecoveryManager если не инициализирован
        if (!recoveryManager) {
            recovery::RecoveryConfig recoveryConfig;
            recoveryConfig.maxRecoveryPoints = 3;
            recoveryConfig.checkpointInterval = std::chrono::seconds(120);
            recoveryConfig.enableAutoRecovery = true;
            recoveryConfig.enableStateValidation = false;
            recoveryConfig.pointConfig.maxSize = 1024 * 1024 * 5;
            recoveryConfig.pointConfig.enableCompression = false;
            recoveryConfig.pointConfig.storagePath = "recovery_points";
            recoveryConfig.pointConfig.retentionPeriod = std::chrono::hours(1);
            recoveryConfig.logPath = "logs/recovery.log";
            recoveryConfig.maxLogSize = 1024 * 1024;
            recoveryConfig.maxLogFiles = 1;
            recoveryManager = std::make_unique<recovery::RecoveryManager>(recoveryConfig);
        }
        
        // Инициализируем DynamicCache если не инициализирован
        if (!dynamicCache) {
            auto cacheConfig = platformOptimizer ? platformOptimizer->getOptimalConfig() : cache::CacheConfig{};
            dynamicCache = std::make_unique<cache::DefaultDynamicCache>(cacheConfig.maxSize);
        }
        
        running_ = true;
        spdlog::info("MicroKernel[{}]: инициализация завершена успешно", id);
    } catch (const std::exception& e) {
        spdlog::error("MicroKernel[{}]: ошибка инициализации: {}", id, e.what());
        ok = false;
    }
    
    return ok;
}

void MicroKernel::shutdown() {
    spdlog::info("MicroKernel[{}]: завершение работы", id);
    running_ = false;
    if (dynamicCache) dynamicCache->clear();
    if (dynamicCache) dynamicCache.reset();
}

bool MicroKernel::executeTask(const std::vector<uint8_t>& data) {
    spdlog::debug("MicroKernel[{}]: выполнение задачи", id);
    dynamicCache->put("task", data);
    recoveryManager->createRecoveryPoint();
    return true;
}

void MicroKernel::updateMetrics() {
    auto json = getMetrics().toJson();
    spdlog::debug("MicroKernel metrics: {}", json.dump());
    updateExtendedMetrics();
}

bool MicroKernel::isRunning() const { 
    std::shared_lock<std::shared_mutex> lock(kernelMutex_);
    return running_; 
}

metrics::PerformanceMetrics MicroKernel::getMetrics() const {
    metrics::PerformanceMetrics m{};
    if (threadPool) {
        auto t = threadPool->getMetrics();
        m.cpu_usage = static_cast<double>(t.activeThreads) / t.totalThreads;
    }
    if (dynamicCache) {
        m.memory_usage = static_cast<double>(dynamicCache->size()) / 1000.0; // Примерная оценка
    }
    m.timestamp = std::chrono::steady_clock::now();
    return m;
}

void MicroKernel::setResourceLimit(const std::string&, double) {}
double MicroKernel::getResourceUsage(const std::string&) const { return 0.0; }
KernelType MicroKernel::getType() const { return KernelType::MICRO; }
std::string MicroKernel::getId() const { return id; }
void MicroKernel::pause() {}
void MicroKernel::resume() {}
void MicroKernel::reset() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    shutdown();
    // После reset ядро не инициализировано, running_ = false
    // Все ресурсы будут пересозданы при следующем initialize
}
std::vector<std::string> MicroKernel::getSupportedFeatures() const { return {}; }

// Новые методы интеграции

void MicroKernel::setPreloadManager(std::shared_ptr<core::PreloadManager> preloadManager) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    preloadManager_ = preloadManager;
    spdlog::info("MicroKernel[{}]: PreloadManager установлен", id);
}

void MicroKernel::warmupFromPreload() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    if (!preloadManager_ || !dynamicCache) {
        spdlog::warn("MicroKernel[{}]: PreloadManager или DynamicCache недоступны для warm-up", id);
        return;
    }
    
    try {
        spdlog::info("MicroKernel[{}]: Начинаем warm-up из PreloadManager", id);
        
        // Получаем все ключи из PreloadManager
        auto keys = preloadManager_->getAllKeys();
        spdlog::debug("MicroKernel[{}]: Получено {} ключей для warm-up", id, keys.size());
        
        // Получаем данные для ключей
        for (const auto& key : keys) {
            auto data = preloadManager_->getDataForKey(key);
            if (data) {
                dynamicCache->put(key, *data);
                spdlog::trace("MicroKernel[{}]: Загружен ключ '{}' в кэш", id, key);
            }
        }
        
        spdlog::info("MicroKernel[{}]: Warm-up завершен, загружено {} элементов", id, keys.size());
        notifyEvent("warmup_completed", keys.size());
        
    } catch (const std::exception& e) {
        spdlog::error("MicroKernel[{}]: Ошибка при warm-up: {}", id, e.what());
        notifyEvent("warmup_failed", std::string(e.what()));
    }
}

ExtendedKernelMetrics MicroKernel::getExtendedMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex_);
    return extendedMetrics_;
}

void MicroKernel::updateExtendedMetrics() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    updateExtendedMetricsFromPerformance();
}

bool MicroKernel::processTask(const balancer::TaskDescriptor& task) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    
    try {
        spdlog::debug("MicroKernel[{}]: Обработка задачи типа {} с приоритетом {}", 
                     id, static_cast<int>(task.type), task.priority);
        
        // Вызываем callback если установлен
        if (taskCallback_) {
            taskCallback_(task);
        }
        
        // Обрабатываем данные через DynamicCache
        if (dynamicCache) {
            std::string key = "task_" + std::to_string(task.priority) + "_" + 
                             std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 task.enqueueTime.time_since_epoch()).count());
            dynamicCache->put(key, task.data);
        }
        
        // Обновляем метрики
        updateExtendedMetrics();
        
        notifyEvent("task_processed", task);
        spdlog::debug("MicroKernel[{}]: Задача успешно обработана", id);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("MicroKernel[{}]: Ошибка обработки задачи: {}", id, e.what());
        notifyEvent("task_failed", std::string(e.what()));
        return false;
    }
}

void MicroKernel::scheduleTask(std::function<void()> task, int priority) {
    if (threadPool) {
        threadPool->enqueue(std::move(task));
    }
}

void MicroKernel::setTaskCallback(TaskCallback callback) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    taskCallback_ = callback;
    spdlog::debug("MicroKernel[{}]: TaskCallback установлен", id);
}

void MicroKernel::setLoadBalancer(std::shared_ptr<balancer::LoadBalancer> loadBalancer) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    loadBalancer_ = loadBalancer;
    spdlog::info("MicroKernel[{}]: LoadBalancer установлен", id);
}

std::shared_ptr<balancer::LoadBalancer> MicroKernel::getLoadBalancer() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex_);
    return loadBalancer_;
}

void MicroKernel::setEventCallback(const std::string& event, EventCallback callback) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    eventCallbacks_[event] = callback;
    spdlog::debug("MicroKernel[{}]: EventCallback установлен для события '{}'", id, event);
}

void MicroKernel::removeEventCallback(const std::string& event) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex_);
    eventCallbacks_.erase(event);
    spdlog::debug("MicroKernel[{}]: EventCallback удален для события '{}'", id, event);
}

void MicroKernel::triggerEvent(const std::string& event, const std::any& data) {
    notifyEvent(event, data);
}

// Protected methods

void MicroKernel::initializePreloadManager() {
    spdlog::debug("MicroKernel[{}]: Инициализация PreloadManager", id);
}

void MicroKernel::initializeLoadBalancer() {
    spdlog::debug("MicroKernel[{}]: Инициализация LoadBalancer", id);
}

void MicroKernel::updateExtendedMetricsFromPerformance() {
    try {
        auto perfMetrics = getMetrics();
        
        // Основные метрики
        extendedMetrics_.load = perfMetrics.cpu_usage;
        extendedMetrics_.latency = 0.0; // latency отсутствует
        extendedMetrics_.cacheEfficiency = 0.0; // cacheEfficiency отсутствует
        extendedMetrics_.tunnelBandwidth = 0.0; // Значение по умолчанию
        extendedMetrics_.activeTasks = threadPool ? threadPool->getQueueSize() : 0;
        
        // Resource-Aware метрики
        extendedMetrics_.cpuUsage = perfMetrics.cpu_usage;
        extendedMetrics_.memoryUsage = perfMetrics.memory_usage;
        extendedMetrics_.networkBandwidth = 1000.0; // Примерное значение
        extendedMetrics_.diskIO = 100.0; // Примерное значение
        extendedMetrics_.energyConsumption = 50.0; // Примерное значение
        
        // Workload-Specific метрики
        extendedMetrics_.cpuTaskEfficiency = 0.8;
        extendedMetrics_.ioTaskEfficiency = 0.7;
        extendedMetrics_.memoryTaskEfficiency = 0.6;
        extendedMetrics_.networkTaskEfficiency = 0.9;
        
    } catch (const std::exception& e) {
        spdlog::error("MicroKernel[{}]: Ошибка обновления расширенных метрик: {}", id, e.what());
    }
}

void MicroKernel::notifyEvent(const std::string& event, const std::any& data) {
    auto it = eventCallbacks_.find(event);
    if (it != eventCallbacks_.end()) {
        try {
            it->second(event, data);
            spdlog::trace("MicroKernel[{}]: Событие '{}' обработано", id, event);
        } catch (const std::exception& e) {
            spdlog::error("MicroKernel[{}]: Ошибка обработки события '{}': {}", id, event, e.what());
        }
    }
}

} // namespace kernel
} // namespace core
} // namespace cloud
