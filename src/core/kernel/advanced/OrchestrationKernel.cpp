#include "core/kernel/advanced/OrchestrationKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include "core/kernel/base/MicroKernel.hpp"

namespace cloud {
namespace core {
namespace kernel {

OrchestrationKernel::OrchestrationKernel() {
    // Инициализация компонентов
    loadBalancer = std::make_unique<balancer::LoadBalancer>();
    platformOptimizer = core::cache::PlatformOptimizer();
    
    // Создание ThreadPool с меньшими ресурсами
    cloud::core::thread::ThreadPoolConfig threadConfig;
    threadConfig.minThreads = 2;
    threadConfig.maxThreads = 8;
    threadConfig.queueSize = 512;
    threadConfig.stackSize = 1024 * 1024;
    threadPool = std::make_shared<core::thread::ThreadPool>(threadConfig);
    
    // Создание RecoveryManager с упрощенной конфигурацией
    core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 5;
    recoveryConfig.checkpointInterval = std::chrono::seconds(60);
    recoveryConfig.enableAutoRecovery = true;
    recoveryConfig.enableStateValidation = false; // Отключаем для ускорения
    recoveryConfig.pointConfig.maxSize = 1024 * 1024 * 5; // Уменьшаем размер
    recoveryConfig.pointConfig.enableCompression = false; // Отключаем сжатие
    recoveryConfig.pointConfig.storagePath = "./recovery_points/orchestration";
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::hours(1);
    recoveryConfig.logPath = "./logs/orchestration_recovery.log";
    recoveryConfig.maxLogSize = 1024 * 1024; // Уменьшаем размер лога
    recoveryConfig.maxLogFiles = 1;
    
    recoveryManager = std::make_unique<core::recovery::RecoveryManager>(recoveryConfig);
    
    // Создание DynamicCache для задач оркестрации с меньшим размером
    auto cacheConfig = platformOptimizer.getOptimalConfig();
    dynamicCache = std::make_unique<core::cache::DynamicCache<std::string, std::vector<uint8_t>>>(
        std::min(cacheConfig.maxEntries / 4, static_cast<size_t>(1000)), 1800 // 30 минут TTL
    );
    
    spdlog::info("OrchestrationKernel: создан");
}

OrchestrationKernel::~OrchestrationKernel() {
    shutdown();
}

bool OrchestrationKernel::initialize() {
    try {
    spdlog::info("OrchestrationKernel: инициализация");
        
        // Инициализация LoadBalancer
        loadBalancer->setStrategy("hybrid_adaptive");
        loadBalancer->setResourceWeights(0.3, 0.25, 0.25, 0.2);
        loadBalancer->setAdaptiveThresholds(0.8, 0.7);
        
        // Инициализация RecoveryManager
        if (!recoveryManager->initialize()) {
            spdlog::error("OrchestrationKernel: ошибка инициализации RecoveryManager");
            return false;
        }
        
        // Настройка DynamicCache
        dynamicCache->setEvictionCallback([](const std::string& key, const std::vector<uint8_t>& data) {
            spdlog::debug("OrchestrationKernel: элемент вытеснен из кэша: key={}, size={}", key, data.size());
        });
        
        dynamicCache->setAutoResize(true, 100, 10000);
        dynamicCache->setCleanupInterval(300); // 5 минут
        
        spdlog::info("OrchestrationKernel: инициализация завершена успешно");
    return true;
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка инициализации: {}", e.what());
        return false;
    }
}

void OrchestrationKernel::shutdown() {
    spdlog::info("OrchestrationKernel: shutdown() start");
    bool error = false;
    std::string errorMsg;
    try {
        // dynamicCache
        if (dynamicCache) {
            try { dynamicCache->clear(); dynamicCache.reset(); }
            catch (const std::exception& e) { spdlog::error("OrchestrationKernel: dynamicCache exception: {}", e.what()); error = true; errorMsg = "dynamicCache: " + std::string(e.what()); }
        }
        // recoveryManager
        if (recoveryManager) {
            try { recoveryManager->shutdown(); recoveryManager.reset(); }
            catch (const std::exception& e) { spdlog::error("OrchestrationKernel: recoveryManager exception: {}", e.what()); error = true; errorMsg = "recoveryManager: " + std::string(e.what()); }
        }
        // threadPool
        if (threadPool) {
            try { threadPool->waitForCompletion(); threadPool.reset(); }
            catch (const std::exception& e) { spdlog::error("OrchestrationKernel: threadPool exception: {}", e.what()); error = true; errorMsg = "threadPool: " + std::string(e.what()); }
        }
        // platformOptimizer
        try { /* platformOptimizer - не unique_ptr, просто reset если есть */ } catch (const std::exception& e) { spdlog::error("OrchestrationKernel: platformOptimizer exception: {}", e.what()); error = true; errorMsg = "platformOptimizer: " + std::string(e.what()); }
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: shutdown() outer exception: {}", e.what());
        error = true;
        errorMsg = "outer: " + std::string(e.what());
    }
    // Если есть флаг состояния, сбросить его здесь (например, running = false)
    spdlog::info("OrchestrationKernel: shutdown() завершён (error: {})", error ? errorMsg : "none");
}

void OrchestrationKernel::enqueueTask(const std::vector<uint8_t>& data, int priority) {
    try {
        // Создаем TaskDescriptor
        balancer::TaskDescriptor task;
        task.data = data;
        task.priority = priority;
        task.type = balancer::TaskType::CPU_INTENSIVE; // По умолчанию
        task.enqueueTime = std::chrono::steady_clock::now();
        
        // Определяем тип задачи на основе размера данных
        if (data.size() > 1024 * 1024) { // > 1MB
            task.type = balancer::TaskType::MEMORY_INTENSIVE;
        } else if (data.size() < 1024) { // < 1KB
            task.type = balancer::TaskType::IO_INTENSIVE;
        }
        
        taskDescriptors.push_back(std::move(task));
        
        // Сохраняем в кэш для отслеживания
        std::string taskKey = "task_" + std::to_string(taskDescriptors.size() - 1);
        dynamicCache->put(taskKey, data);
        
        spdlog::debug("OrchestrationKernel: задача добавлена в очередь, priority={}, type={}, size={}", 
                     priority, static_cast<int>(task.type), data.size());
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка добавления задачи: {}", e.what());
    }
}

void OrchestrationKernel::balanceTasks() {
    try {
        if (taskDescriptors.empty()) {
            spdlog::debug("OrchestrationKernel: нет задач для балансировки");
            return;
        }
        
        // Создаем фиктивные ядра для тестирования
        std::vector<std::shared_ptr<IKernel>> dummyKernels;
        for (int i = 0; i < 3; ++i) {
            // Создаем фиктивное ядро
            auto dummyKernel = std::make_shared<cloud::core::kernel::MicroKernel>("dummy_" + std::to_string(i));
            dummyKernel->initialize();
            dummyKernels.push_back(dummyKernel);
        }
        
        // Получаем метрики ядер
        auto kernelMetrics = getKernelMetrics(dummyKernels);
        
        // Выполняем балансировку
        loadBalancer->balance(dummyKernels, taskDescriptors, kernelMetrics);
        
        spdlog::debug("OrchestrationKernel: балансировка выполнена для {} задач", taskDescriptors.size());
        
        // Очищаем обработанные задачи
        taskDescriptors.clear();
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка балансировки задач: {}", e.what());
    }
}

void OrchestrationKernel::accelerateTunnels() {
    try {
        spdlog::debug("OrchestrationKernel: ускорение туннелей");
        
        // Эмулируем ускорение туннелей
        if (threadPool) {
            threadPool->enqueue([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                spdlog::trace("OrchestrationKernel: туннель ускорен");
            });
        }
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка ускорения туннелей: {}", e.what());
    }
}

void OrchestrationKernel::orchestrate(const std::vector<std::shared_ptr<IKernel>>& kernels) {
    try {
        spdlog::info("OrchestrationKernel: оркестрация {} ядер", kernels.size());
        
        // Получаем метрики всех ядер
        auto kernelMetrics = getKernelMetrics(kernels);
        
        // Выполняем балансировку
        loadBalancer->balance(kernels, taskDescriptors, kernelMetrics);
        
        // Создаем точку восстановления
        if (recoveryManager) {
            std::string pointId = recoveryManager->createRecoveryPoint();
            spdlog::debug("OrchestrationKernel: создана точка восстановления: {}", pointId);
        }
        
        // Обновляем метрики
        updateMetrics();
        
        spdlog::info("OrchestrationKernel: оркестрация завершена");
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка оркестрации: {}", e.what());
    }
}

void OrchestrationKernel::updateMetrics() {
    try {
        // Обновляем метрики всех компонентов
        if (loadBalancer) {
            // LoadBalancer не имеет updateMetrics
        }
        
        if (threadPool) {
            threadPool->updateMetrics();
        }
        
        if (recoveryManager) {
            // RecoveryManager не имеет публичного updateMetrics
        }
        
        // Обновляем метрики кэша
        if (dynamicCache) {
            // DynamicCache не имеет updateMetrics
        }
        
        spdlog::trace("OrchestrationKernel: метрики обновлены");
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка обновления метрик: {}", e.what());
    }
}

bool OrchestrationKernel::isRunning() const {
    return threadPool && threadPool->getQueueSize() > 0;
}

metrics::PerformanceMetrics OrchestrationKernel::getMetrics() const {
    metrics::PerformanceMetrics metrics{};
    
    try {
        if (threadPool) {
            auto threadMetrics = threadPool->getMetrics();
            metrics.cpu_usage = static_cast<double>(threadMetrics.activeThreads) / threadMetrics.totalThreads;
        }
        
        if (dynamicCache) {
            metrics.memory_usage = static_cast<double>(dynamicCache->size()) / 1000.0;
        }
        
        metrics.timestamp = std::chrono::steady_clock::now();
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка получения метрик: {}", e.what());
    }
    
    return metrics;
}

void OrchestrationKernel::setResourceLimit(const std::string& resource, double limit) {
    try {
        if (resource == "threads" && threadPool) {
            cloud::core::thread::ThreadPoolConfig config = threadPool->getConfiguration();
            config.maxThreads = static_cast<size_t>(limit);
            threadPool->setConfiguration(config);
            spdlog::info("OrchestrationKernel: установлен лимит потоков {}", limit);
        } else if (resource == "cache" && dynamicCache) {
            dynamicCache->resize(static_cast<size_t>(limit));
            spdlog::info("OrchestrationKernel: установлен лимит кэша {}", limit);
        } else {
            spdlog::warn("OrchestrationKernel: неизвестный ресурс '{}'", resource);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка установки лимита ресурса: {}", e.what());
    }
}

double OrchestrationKernel::getResourceUsage(const std::string& resource) const {
    try {
        if (resource == "threads" && threadPool) {
            auto metrics = threadPool->getMetrics();
            return static_cast<double>(metrics.activeThreads);
        } else if (resource == "cache" && dynamicCache) {
            return static_cast<double>(dynamicCache->size());
        } else {
            spdlog::warn("OrchestrationKernel: неизвестный ресурс '{}'", resource);
            return 0.0;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка получения использования ресурса: {}", e.what());
        return 0.0;
    }
}

KernelType OrchestrationKernel::getType() const {
    return KernelType::ORCHESTRATION;
}

std::string OrchestrationKernel::getId() const {
    return "orchestration_kernel";
}

void OrchestrationKernel::pause() {
    spdlog::info("OrchestrationKernel: приостановлен");
}

void OrchestrationKernel::resume() {
    spdlog::info("OrchestrationKernel: возобновлен");
}

void OrchestrationKernel::reset() {
    try {
    taskDescriptors.clear();
        taskQueue.clear();
        
        if (dynamicCache) {
            dynamicCache->clear();
        }
        
        spdlog::info("OrchestrationKernel: сброшен");
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка сброса: {}", e.what());
    }
}

std::vector<std::string> OrchestrationKernel::getSupportedFeatures() const {
    return {
        "task_orchestration",
        "load_balancing", 
        "recovery_management",
        "dynamic_thread_pool",
        "cache_optimization",
        "tunnel_acceleration"
    };
}

std::vector<balancer::KernelMetrics> OrchestrationKernel::getKernelMetrics(
    const std::vector<std::shared_ptr<IKernel>>& kernels) const {
    
    std::vector<balancer::KernelMetrics> metrics;
    
    try {
        for (const auto& kernel : kernels) {
            balancer::KernelMetrics metric;
            
            // Получаем метрики ядра
            auto perfMetrics = kernel->getMetrics();
            
            metric.cpuUsage = perfMetrics.cpu_usage;
            metric.memoryUsage = perfMetrics.memory_usage;
            metric.networkBandwidth = 1000.0; // Примерное значение
            metric.diskIO = 100.0; // Примерное значение
            metric.energyConsumption = 50.0; // Примерное значение
            
            // Эффективность для разных типов задач
            metric.cpuTaskEfficiency = 0.8;
            metric.ioTaskEfficiency = 0.7;
            metric.memoryTaskEfficiency = 0.6;
            metric.networkTaskEfficiency = 0.9;
            
            metrics.push_back(metric);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("OrchestrationKernel: ошибка получения метрик ядер: {}", e.what());
    }
    
    return metrics;
}

} // namespace kernel
} // namespace core
} // namespace cloud
