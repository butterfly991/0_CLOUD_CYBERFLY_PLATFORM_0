#include "core/kernel/advanced/SmartKernel.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <random>
#include <iomanip>
#include <filesystem>
#include <future>
#include <spdlog/spdlog.h>

// Платформо-зависимые определения
#if defined(__APPLE__) && defined(__arm64__)
    #define PLATFORM_APPLE_ARM
    #include <mach/mach.h>
    #include <mach/processor_info.h>
    #include <mach/mach_host.h>
#elif defined(__linux__) && defined(__x86_64__)
    #define PLATFORM_LINUX_X64
    #include <sys/sysinfo.h>
    #include <sys/statvfs.h>
    #include <fstream>
#endif

namespace cloud {
namespace core {
namespace kernel {

namespace detail {

class PerformanceMonitor {
public:
    explicit PerformanceMonitor(const SmartKernelConfig& config)
        : config_(config) {
        initializeMetrics();
        initializeLogger();
    }
    
    void updateMetrics() {
        try {
            #if defined(PLATFORM_APPLE_ARM)
                updateAppleMetrics();
            #elif defined(PLATFORM_LINUX_X64)
                updateLinuxMetrics();
            #else
                updateGenericMetrics();
            #endif
            
            calculateEfficiency();
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->error("Failed to update metrics: {}", e.what());
            }
        }
    }
    
    metrics::AdaptiveMetrics getMetrics() const {
        std::shared_lock<std::shared_mutex> lock(metricsMutex_);
        return metrics_;
    }
    
private:
    void initializeMetrics() {
        metrics_ = metrics::AdaptiveMetrics{};
    }
    
    void initializeLogger() {
        try {
            logger_ = spdlog::get("smartkernel_performance");
            if (!logger_) {
                logger_ = spdlog::rotating_logger_mt("smartkernel_performance", 
                    "logs/smartkernel_performance.log", 1024 * 1024 * 5, 2);
            }
        } catch (const std::exception& e) {
            // Fallback to default logger
            logger_ = spdlog::default_logger();
        }
    }
    
    #if defined(PLATFORM_APPLE_ARM)
    void updateAppleMetrics() {
        host_cpu_load_info_data_t cpuInfo;
        mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
        if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                           (host_info_t)&cpuInfo, &count) == KERN_SUCCESS) {
            uint64_t total = cpuInfo.cpu_ticks[CPU_STATE_IDLE] +
                                                  cpuInfo.cpu_ticks[CPU_STATE_USER] +
                           cpuInfo.cpu_ticks[CPU_STATE_SYSTEM];
            if (total > 0) {
                metrics_.loadFactor = static_cast<double>(
                    cpuInfo.cpu_ticks[CPU_STATE_USER] + cpuInfo.cpu_ticks[CPU_STATE_SYSTEM]) / total;
            }
        }
        
        // Получаем информацию о памяти
        vm_statistics64_data_t vmStats;
        count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                             (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
            uint64_t totalMemory = vmStats.active_count + vmStats.inactive_count + vmStats.free_count;
            if (totalMemory > 0) {
                metrics_.resourceUtilization = static_cast<double>(vmStats.active_count) / totalMemory;
            }
        }
    }
    #elif defined(PLATFORM_LINUX_X64)
    void updateLinuxMetrics() {
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            metrics_.loadFactor = static_cast<double>(si.loads[0]) / 65536.0;
        }
        
        // Читаем информацию о температуре
        std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
        if (tempFile.is_open()) {
            int temp;
            tempFile >> temp;
            metrics_.thermalEfficiency = 1.0 - (static_cast<double>(temp) / 100000.0);
        }
        
        // Читаем информацию о памяти
        std::ifstream memFile("/proc/meminfo");
        if (memFile.is_open()) {
            std::string line;
            uint64_t totalMem = 0, freeMem = 0;
            while (std::getline(memFile, line)) {
                if (line.find("MemTotal:") == 0) {
                    sscanf(line.c_str(), "MemTotal: %lu", &totalMem);
                } else if (line.find("MemAvailable:") == 0) {
                    sscanf(line.c_str(), "MemAvailable: %lu", &freeMem);
                }
            }
            if (totalMem > 0) {
                metrics_.resourceUtilization = 1.0 - (static_cast<double>(freeMem) / totalMem);
            }
        }
    }
    #else
    void updateGenericMetrics() {
        // Генерируем синтетические метрики для неподдерживаемых платформ
        static std::mt19937 rng{std::random_device{}()};
        static std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        metrics_.loadFactor = dist(rng);
        metrics_.resourceUtilization = dist(rng);
        metrics_.thermalEfficiency = 0.8; // Предполагаем хорошую температуру
        metrics_.powerEfficiency = 0.7;   // Предполагаем среднюю эффективность
    }
    #endif
    
    void calculateEfficiency() {
        metrics_.efficiencyScore = (metrics_.loadFactor * 0.3 +
                                  metrics_.powerEfficiency * 0.3 +
                                  metrics_.thermalEfficiency * 0.2 +
                                  metrics_.resourceUtilization * 0.2);
    }
    
    SmartKernelConfig config_;
    metrics::AdaptiveMetrics metrics_;
    mutable std::shared_mutex metricsMutex_;
    std::shared_ptr<spdlog::logger> logger_;
};

class ResourceManager {
public:
    explicit ResourceManager(const config::ResourceConfig& config)
        : config_(config) {
        initializeResources();
    }
    
    bool allocateResource(const std::string& resource, double amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it == resources_.end()) return false;
        
        if (it->second.current + amount > it->second.limit) return false;
        
        it->second.current += amount;
        return true;
    }
    
    void deallocateResource(const std::string& resource, double amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it == resources_.end()) return;
        
        it->second.current = std::max(0.0, it->second.current - amount);
    }
    
    double getResourceEfficiency(const std::string& resource) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it == resources_.end()) return 0.0;
        
        return it->second.current / it->second.limit;
    }
    
    config::ResourceConfig getConfig() const {
        return config_;
    }
    
private:
    void initializeResources() {
        resources_["cpu"] = {config_.cpuLimit, 0.0};
        resources_["memory"] = {static_cast<double>(config_.memoryLimit), 0.0};
        resources_["cache"] = {static_cast<double>(config_.cacheSize), 0.0};
    }
    
    struct Resource {
        double limit;
        double current;
    };
    
    config::ResourceConfig config_;
    std::unordered_map<std::string, Resource> resources_;
    mutable std::mutex mutex_;
};

class AdaptiveController {
public:
    explicit AdaptiveController(const config::AdaptiveConfig& config)
        : config_(config) {
        initializeController();
    }
    
    void update(const metrics::AdaptiveMetrics& metrics) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        metricsHistory_.push_back(metrics);
        if (metricsHistory_.size() > config_.historySize) {
            metricsHistory_.pop_front();
        }
        
        if (shouldAdapt()) {
            adapt();
        }
    }
    
    std::vector<double> getAdaptationParameters() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentParameters_;
    }
    
private:
    void initializeController() {
        currentParameters_ = {
            config_.learningRate,
            config_.explorationRate,
            0.5, // threadPoolSize
            0.5, // cacheSize
            0.5  // recoveryInterval
        };
    }
    
    bool shouldAdapt() const {
        if (metricsHistory_.size() < 2) return false;
        
        auto now = std::chrono::steady_clock::now();
        auto lastAdaptation = metricsHistory_.back().lastAdaptation;
        
        return (now - lastAdaptation) >= config_.adaptationInterval;
    }
    
    void adapt() {
        if (metricsHistory_.size() < 2) return;
        
        double gradient = calculateGradient();
        
        // Обновляем параметры
        for (auto& param : currentParameters_) {
            param += config_.learningRate * gradient;
            param = std::clamp(param, 0.0, 1.0);
        }
        
        // Иногда исследуем новые области
        if (std::uniform_real_distribution<>(0.0, 1.0)(rng_) < config_.explorationRate) {
            explore();
        }
    }
    
    double calculateGradient() const {
        if (metricsHistory_.size() < 2) return 0.0;
        
        auto& current = metricsHistory_.back();
        auto& previous = *(metricsHistory_.end() - 2);
        
        return current.efficiencyScore - previous.efficiencyScore;
    }
    
    void explore() {
        std::uniform_real_distribution<> dist(-0.1, 0.1);
        for (auto& param : currentParameters_) {
            param += dist(rng_);
            param = std::clamp(param, 0.0, 1.0);
        }
    }
    
    config::AdaptiveConfig config_;
    std::deque<metrics::AdaptiveMetrics> metricsHistory_;
    std::vector<double> currentParameters_;
    std::mt19937 rng_{std::random_device{}()};
    mutable std::mutex mutex_;
};

} // namespace detail

struct SmartKernel::Impl {
    SmartKernelConfig config;
    std::function<void(const std::string&)> errorCallback;
    std::chrono::steady_clock::time_point lastMetricsUpdate;
    
    Impl(const SmartKernelConfig& cfg)
        : config(cfg)
        , lastMetricsUpdate(std::chrono::steady_clock::now()) {}
};

SmartKernel::SmartKernel(const SmartKernelConfig& config)
    : pImpl(std::make_unique<Impl>(config)) {
    initializeLogger();
}

SmartKernel::~SmartKernel() {
    shutdown();
}

SmartKernel::SmartKernel(SmartKernel&& other) noexcept
    : pImpl(std::move(other.pImpl))
    , performanceMonitor(std::move(other.performanceMonitor))
    , resourceManager(std::move(other.resourceManager))
    , adaptiveController(std::move(other.adaptiveController))
    , threadPool(std::move(other.threadPool))
    , recoveryManager(std::move(other.recoveryManager))
    , dynamicCache(std::move(other.dynamicCache))
    , platformOptimizer(std::move(other.platformOptimizer))
    , initialized(other.initialized.load())
    , isOptimizing(other.isOptimizing.load()) {
}

SmartKernel& SmartKernel::operator=(SmartKernel&& other) noexcept {
    if (this != &other) {
        shutdown();
        pImpl = std::move(other.pImpl);
        performanceMonitor = std::move(other.performanceMonitor);
        resourceManager = std::move(other.resourceManager);
        adaptiveController = std::move(other.adaptiveController);
        threadPool = std::move(other.threadPool);
        recoveryManager = std::move(other.recoveryManager);
        dynamicCache = std::move(other.dynamicCache);
        platformOptimizer = std::move(other.platformOptimizer);
        initialized = other.initialized.load();
        isOptimizing = other.isOptimizing.load();
    }
    return *this;
}

bool SmartKernel::initialize() {
    std::lock_guard<std::shared_mutex> lock(kernelMutex);
    
    if (initialized.load()) {
        spdlog::warn("SmartKernel: повторная инициализация");
        return true;
    }
    
    try {
        spdlog::info("SmartKernel: initializeComponents start");
        initializeComponents();
        initialized.store(true);
        spdlog::info("SmartKernel: успешно инициализирован");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("SmartKernel: Ошибка инициализации: {}", e.what());
        handleError("Ошибка инициализации: " + std::string(e.what()));
        return false;
    } catch (...) {
        spdlog::error("SmartKernel: Неизвестная ошибка инициализации");
        handleError("Неизвестная ошибка инициализации");
        return false;
    }
}

void SmartKernel::shutdown() {
    std::lock_guard<std::shared_mutex> lock(kernelMutex);
    if (!initialized.load()) {
        spdlog::info("SmartKernel: shutdown() вызван повторно, ядро уже остановлено");
        return;
    }
    spdlog::info("SmartKernel: shutdown() start");
    bool error = false;
    std::string errorMsg;
    try {
        // dynamicCache
        if (dynamicCache) {
            try { dynamicCache->clear(); dynamicCache.reset(); }
            catch (const std::exception& e) { spdlog::error("SmartKernel: dynamicCache exception: {}", e.what()); error = true; errorMsg = "dynamicCache: " + std::string(e.what()); }
        }
        // recoveryManager
        if (recoveryManager) {
            try { recoveryManager->shutdown(); recoveryManager.reset(); }
            catch (const std::exception& e) { spdlog::error("SmartKernel: recoveryManager exception: {}", e.what()); error = true; errorMsg = "recoveryManager: " + std::string(e.what()); }
        }
        // threadPool
        if (threadPool) {
            try { threadPool->waitForCompletion(); threadPool.reset(); }
            catch (const std::exception& e) { spdlog::error("SmartKernel: threadPool exception: {}", e.what()); error = true; errorMsg = "threadPool: " + std::string(e.what()); }
        }
        // performanceMonitor
        if (performanceMonitor) {
            try { performanceMonitor.reset(); }
            catch (const std::exception& e) { spdlog::error("SmartKernel: performanceMonitor exception: {}", e.what()); error = true; errorMsg = "performanceMonitor: " + std::string(e.what()); }
        }
        // resourceManager
        if (resourceManager) {
            try { resourceManager.reset(); }
            catch (const std::exception& e) { spdlog::error("SmartKernel: resourceManager exception: {}", e.what()); error = true; errorMsg = "resourceManager: " + std::string(e.what()); }
        }
        // adaptiveController
        if (adaptiveController) {
            try { adaptiveController.reset(); }
            catch (const std::exception& e) { spdlog::error("SmartKernel: adaptiveController exception: {}", e.what()); error = true; errorMsg = "adaptiveController: " + std::string(e.what()); }
        }
        // platformOptimizer
        try { /* platformOptimizer - не unique_ptr, просто reset если есть */ } catch (const std::exception& e) { spdlog::error("SmartKernel: platformOptimizer exception: {}", e.what()); error = true; errorMsg = "platformOptimizer: " + std::string(e.what()); }
    } catch (const std::exception& e) {
        spdlog::error("SmartKernel: shutdown() outer exception: {}", e.what());
        error = true;
        errorMsg = "outer: " + std::string(e.what());
    }
    initialized.store(false);
    spdlog::info("SmartKernel: shutdown() завершён (error: {})", error ? errorMsg : "none");
}

metrics::PerformanceMetrics SmartKernel::getMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    
    if (!initialized.load()) {
        return metrics::PerformanceMetrics{};
    }
    
    return CoreKernel::getMetrics();
}

void SmartKernel::updateMetrics() {
    std::lock_guard<std::shared_mutex> lock(kernelMutex);
    
    if (!initialized.load()) {
        return;
    }
    
    try {
        if (performanceMonitor) {
            performanceMonitor->updateMetrics();
        }
        
        if (adaptiveController) {
            auto metrics = performanceMonitor ? performanceMonitor->getMetrics() : metrics::AdaptiveMetrics{};
            adaptiveController->update(metrics);
            
            // Адаптируем компоненты
            adaptThreadPool(metrics);
            adaptCacheSize(metrics);
            adaptRecovery();
        }
        
        CoreKernel::updateMetrics();
        pImpl->lastMetricsUpdate = std::chrono::steady_clock::now();
        
    } catch (const std::exception& e) {
        handleError("Ошибка обновления метрик: " + std::string(e.what()));
    }
}

void SmartKernel::setConfiguration(const SmartKernelConfig& config) {
    std::lock_guard<std::shared_mutex> lock(kernelMutex);

    if (!config.validate()) {
        handleError("Некорректная конфигурация SmartKernel");
        return;
    }

    spdlog::info("SmartKernel: применяется новая конфигурация: maxThreads={}, maxMemory={}, metricsInterval={}, adaptationThreshold={}",
        config.maxThreads, config.maxMemory, config.metricsInterval.count(), config.adaptationThreshold);

    // Завершаем старые компоненты (shutdownComponents гарантирует устойчивость)
    try {
        shutdownComponents();
    } catch (const std::exception& e) {
        spdlog::error("SmartKernel: ошибка при завершении старых компонентов: {}", e.what());
    }

    // Сохраняем новый конфиг
    pImpl->config = config;

    // Пересоздаём компоненты с новым конфигом
    try {
        initializeComponents();
        spdlog::info("SmartKernel: компоненты пересозданы с новой конфигурацией");
    } catch (const std::exception& e) {
        handleError("Ошибка при пересоздании компонентов SmartKernel: " + std::string(e.what()));
        // Ядро не инициализировано, компоненты сброшены
    }
}

SmartKernelConfig SmartKernel::getConfiguration() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->config;
}

SmartKernelMetrics SmartKernel::getSmartMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    
    SmartKernelMetrics metrics;
    
    if (threadPool) {
        auto threadMetrics = threadPool->getMetrics();
        metrics.threadUtilization = static_cast<double>(threadMetrics.activeThreads) / threadMetrics.totalThreads;
    }
    
    if (resourceManager) {
        metrics.memoryUtilization = resourceManager->getResourceEfficiency("memory");
        metrics.cacheEfficiency = resourceManager->getResourceEfficiency("cache");
    }
    
    // Вычисляем общую эффективность
    metrics.overallEfficiency = (metrics.threadUtilization * 0.3 +
                               metrics.memoryUtilization * 0.3 +
                               metrics.cacheEfficiency * 0.2 +
                               metrics.preloadEfficiency * 0.1 +
                               metrics.recoveryEfficiency * 0.1);
    
    return metrics;
}

metrics::AdaptiveMetrics SmartKernel::getAdaptiveMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    
    if (performanceMonitor) {
        return performanceMonitor->getMetrics();
    }
    
    return metrics::AdaptiveMetrics{};
}

void SmartKernel::setErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::shared_mutex> lock(kernelMutex);
    pImpl->errorCallback = std::move(callback);
}

void SmartKernel::initializeLogger() {
    try {
        auto logger = spdlog::get("smartkernel");
        if (!logger) {
            logger = spdlog::rotating_logger_mt("smartkernel", "logs/smartkernel.log",
                                               1024 * 1024 * 5, 2);
        }
        logger->set_level(spdlog::level::debug);
    } catch (const std::exception& e) {
        spdlog::error("Ошибка инициализации логгера SmartKernel: {}", e.what());
    }
}

void SmartKernel::initializeComponents() {
    // Валидация конфигов
    if (!pImpl->config.validate()) {
        spdlog::error("SmartKernel: Некорректная конфигурация: maxThreads={}, maxMemory={}, metricsInterval={}, adaptationThreshold={}",
            pImpl->config.maxThreads, pImpl->config.maxMemory, pImpl->config.metricsInterval.count(), pImpl->config.adaptationThreshold);
        throw std::runtime_error("SmartKernel: Некорректная конфигурация");
    }
    if (!pImpl->config.resourceConfig.validate()) {
        spdlog::error("SmartKernel: Некорректная resourceConfig: minThreads={}, maxThreads={}, cacheSize={}, memoryLimit={}, cpuLimit={}",
            pImpl->config.resourceConfig.minThreads, pImpl->config.resourceConfig.maxThreads, pImpl->config.resourceConfig.cacheSize, pImpl->config.resourceConfig.memoryLimit, pImpl->config.resourceConfig.cpuLimit);
        throw std::runtime_error("SmartKernel: Некорректная resourceConfig");
    }
    if (!pImpl->config.adaptiveConfig.validate()) {
        spdlog::error("SmartKernel: Некорректная adaptiveConfig");
        throw std::runtime_error("SmartKernel: Некорректная adaptiveConfig");
    }
    // Дефолтные значения
    if (pImpl->config.resourceConfig.cacheSize == 0) {
        pImpl->config.resourceConfig.cacheSize = 1024 * 1024;
        spdlog::warn("SmartKernel: cacheSize был 0, установлен по умолчанию 1MB");
    }
    // RecoveryConfig
    core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 10;
    recoveryConfig.checkpointInterval = std::chrono::seconds(30);
    recoveryConfig.enableAutoRecovery = true;
    // Дефолтный storagePath
    if (recoveryConfig.pointConfig.storagePath.empty()) {
        recoveryConfig.pointConfig.storagePath = "./recovery_points/smart";
        spdlog::warn("SmartKernel: storagePath для RecoveryManager был пуст, установлен './recovery_points/smart'");
    }
    recoveryConfig.pointConfig.maxSize = 1024 * 1024 * 10;
    recoveryConfig.pointConfig.enableCompression = false;
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::hours(1);
    recoveryConfig.logPath = "./logs/smartkernel_recovery.log";
    recoveryConfig.maxLogSize = 1024 * 1024;
    recoveryConfig.maxLogFiles = 1;
    // Логируем параметры
    spdlog::info("SmartKernel: параметры threadPool: minThreads={}, maxThreads={}", pImpl->config.resourceConfig.minThreads, pImpl->config.resourceConfig.maxThreads);
    spdlog::info("SmartKernel: параметры dynamicCache: cacheSize={}", pImpl->config.resourceConfig.cacheSize);
    spdlog::info("SmartKernel: параметры RecoveryManager: storagePath={}", recoveryConfig.pointConfig.storagePath);
    // ... далее создание компонентов ...
    performanceMonitor = std::make_shared<detail::PerformanceMonitor>(pImpl->config);
    resourceManager = std::make_shared<detail::ResourceManager>(pImpl->config.resourceConfig);
    adaptiveController = std::make_shared<detail::AdaptiveController>(pImpl->config.adaptiveConfig);
    core::thread::ThreadPoolConfig threadConfig;
    threadConfig.minThreads = pImpl->config.resourceConfig.minThreads;
    threadConfig.maxThreads = pImpl->config.resourceConfig.maxThreads;
    threadPool = std::make_shared<core::thread::ThreadPool>(threadConfig);
    recoveryManager = std::make_unique<core::recovery::RecoveryManager>(recoveryConfig);
    dynamicCache = std::make_unique<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>>(
        pImpl->config.resourceConfig.cacheSize);
    platformOptimizer = cloud::core::cache::PlatformOptimizer{};
}

void SmartKernel::shutdownComponents() {
    if (threadPool) {
        threadPool->stop();
    }
    
    if (recoveryManager) {
        recoveryManager->shutdown();
    }
    
    performanceMonitor.reset();
    resourceManager.reset();
    adaptiveController.reset();
    threadPool.reset();
    recoveryManager.reset();
    dynamicCache.reset();
    
    spdlog::info("SmartKernel: компоненты завершены");
}

void SmartKernel::handleError(const std::string& error) {
    spdlog::error("SmartKernel: {}", error);
    
    if (pImpl->errorCallback) {
        try {
            pImpl->errorCallback(error);
        } catch (...) {
            spdlog::error("SmartKernel: ошибка в error callback");
        }
    }
}

void SmartKernel::adaptThreadPool(const metrics::AdaptiveMetrics& metrics) {
    if (!threadPool || !adaptiveController) return;
    
    auto params = adaptiveController->getAdaptationParameters();
    if (params.size() >= 3) {
        size_t newSize = static_cast<size_t>(params[2] * pImpl->config.resourceConfig.maxThreads);
        newSize = std::clamp(newSize, pImpl->config.resourceConfig.minThreads, 
                           pImpl->config.resourceConfig.maxThreads);
        
        // В реальной реализации здесь была бы логика изменения размера пула
        spdlog::debug("SmartKernel: адаптация пула потоков до {} потоков", newSize);
    }
}

void SmartKernel::adaptCacheSize(const metrics::AdaptiveMetrics& metrics) {
    if (!dynamicCache || !adaptiveController) return;
    
    auto params = adaptiveController->getAdaptationParameters();
    if (params.size() >= 4) {
        size_t newSize = static_cast<size_t>(params[3] * pImpl->config.resourceConfig.cacheSize);
        newSize = std::clamp(newSize, static_cast<size_t>(1024 * 1024), pImpl->config.resourceConfig.cacheSize);
        
        // В реальной реализации здесь была бы логика изменения размера кэша
        spdlog::debug("SmartKernel: адаптация размера кэша до {} байт", newSize);
    }
}

void SmartKernel::adaptRecovery() {
    if (!recoveryManager || !adaptiveController) return;
    
    auto params = adaptiveController->getAdaptationParameters();
    if (params.size() >= 5) {
        auto newInterval = std::chrono::seconds(static_cast<int>(params[4] * 60));
        
        // В реальной реализации здесь была бы логика изменения интервала восстановления
        spdlog::debug("SmartKernel: адаптация интервала восстановления до {} секунд", newInterval.count());
    }
}

bool SmartKernel::isRunning() const {
    return initialized.load();
}

} // namespace kernel
} // namespace core
} // namespace cloud

