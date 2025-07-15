#include "core/kernel/base/CoreKernel.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "core/cache/dynamic/DynamicCache.hpp"
#include <random>
#include <unordered_set>
#include "core/balancer/TaskTypes.hpp"
#include "core/cache/CacheConfig.hpp"

namespace cloud {
namespace core {
namespace kernel {

namespace detail {

class PerformanceMonitor {
public:
    explicit PerformanceMonitor(const config::OptimizationConfig& config)
        : config_(config) {
        std::cout << "[DEBUG] PerformanceMonitor ctor start, this=" << this << "\n";
        initializeMetrics();
        std::cout << "[DEBUG] PerformanceMonitor after initializeMetrics\n";
        try {
            auto logger = spdlog::get("performance_monitor");
            if (!logger) {
                logger = spdlog::rotating_logger_mt("performance_monitor", "logs/performance_monitor.log", 1024 * 1024 * 5, 2);
            }
            if (logger) logger->set_level(spdlog::level::debug);
            std::cout << "[DEBUG] PerformanceMonitor logger_ OK\n";
        } catch (const std::exception& e) {
            std::cerr << "[DEBUG] PerformanceMonitor logger_ FAIL: " << e.what() << std::endl;
        }
        std::cout << "[DEBUG] PerformanceMonitor ctor end\n";
    }
    ~PerformanceMonitor() noexcept {
        try {
            std::cerr << "[DEBUG] ~PerformanceMonitor ENTER (noexcept), this=" << this << "\n";
            // Не трогаем logger вообще!
            std::cerr << "[DEBUG] ~PerformanceMonitor EXIT (noexcept)\n";
        } catch (...) {
            // Никогда не бросаем исключения из деструктора
        }
    }

    void updateMetrics() {
        std::shared_lock<std::shared_mutex> lock(metricsMutex_);
        try {
            #ifdef CLOUD_PLATFORM_APPLE_ARM
                updateAppleMetrics();
            #elif defined(CLOUD_PLATFORM_LINUX_X64)
                updateLinuxMetrics();
            #endif
            calculateEfficiency();
        } catch (const std::exception& e) {
            auto logger = spdlog::get("performance_monitor");
            if (logger) logger->error("Failed to update metrics: {}", e.what());
        }
    }

    metrics::PerformanceMetrics getMetrics() const {
        std::shared_lock<std::shared_mutex> lock(metricsMutex_);
        return metrics_;
    }

private:
    void initializeMetrics() {
        metrics_ = metrics::PerformanceMetrics{
            .cpu_usage = 0.0,
            .memory_usage = 0.0,
            .power_consumption = 0.0,
            .temperature = 0.0,
            .instructions_per_second = 0,
            .timestamp = std::chrono::steady_clock::now()
        };
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            metrics_.performance_core_usage = 0.0;
            metrics_.efficiency_core_usage = 0.0;
            metrics_.gpu_usage = 0.0;
            metrics_.neural_engine_usage = 0.0;
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            metrics_.physical_core_usage = 0.0;
            metrics_.logical_core_usage = 0.0;
            metrics_.gpu_usage = 0.0;
            metrics_.avx_usage = 0.0;
        #endif
    }

    #ifdef CLOUD_PLATFORM_APPLE_ARM
    void updateAppleMetrics() {
        processor_cpu_load_info_t cpuLoad;
        mach_msg_type_number_t count;
        host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                           reinterpret_cast<processor_info_t*>(&cpuLoad), &count);

        double perfUsage = 0.0, effUsage = 0.0;
        size_t perfCores = 0, effCores = 0;
        for (size_t i = 0; i < count; ++i) {
            double usage = (cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                          cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM]) /
                         static_cast<double>(cpuLoad[i].cpu_ticks[CPU_STATE_IDLE]);
            
            if (i < 4) {
                perfUsage += usage;
                ++perfCores;
            } else {
                effUsage += usage;
                ++effCores;
            }
        }

        metrics_.performance_core_usage = perfCores > 0 ? perfUsage / perfCores : 0.0;
        metrics_.efficiency_core_usage = effCores > 0 ? effUsage / effCores : 0.0;
        metrics_.cpu_usage = (perfUsage + effUsage) / count;
        
        vm_size_t pageSize;
        host_page_size(mach_host_self(), &pageSize);
        
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);
        host_statistics64(mach_host_self(), HOST_VM_INFO64,
                         reinterpret_cast<host_info64_t>(&vmStats), &infoCount);
        
        uint64_t totalMemory = vmStats.free_count + vmStats.active_count +
                              vmStats.inactive_count + vmStats.wire_count;
        metrics_.memory_usage = 1.0 - (static_cast<double>(vmStats.free_count) / totalMemory);
        
        int power, temp;
        size_t size = sizeof(power);
        if (sysctlbyname("machdep.cpu.power", &power, &size, nullptr, 0) == 0) {
            metrics_.power_consumption = power / 1000.0;
        }
        
        size = sizeof(temp);
        if (sysctlbyname("machdep.cpu.temperature", &temp, &size, nullptr, 0) == 0) {
            metrics_.temperature = temp / 100.0;
        }
        
        size_t neuralEngineUsage;
        size = sizeof(neuralEngineUsage);
        if (sysctlbyname("machdep.cpu.neural_engine_usage", &neuralEngineUsage, &size, nullptr, 0) == 0) {
            metrics_.neural_engine_usage = neuralEngineUsage / 100.0;
        }
    }
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
    void updateLinuxMetrics() {
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            metrics_.memory_usage = 1.0 - (static_cast<double>(si.freeram) / si.totalram);
        }

        std::ifstream statFile("/proc/stat");
        std::string line;
        if (std::getline(statFile, line)) {
            std::istringstream iss(line);
            std::string cpu;
            uint64_t user, nice, system, idle, iowait, irq, softirq;
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
            
            uint64_t total = user + nice + system + idle + iowait + irq + softirq;
            uint64_t idleTotal = idle + iowait;
            
            metrics_.cpu_usage = 1.0 - (static_cast<double>(idleTotal) / total);
        }

        std::ifstream cpuInfoFile("/proc/cpuinfo");
        size_t physicalCores = 0;
        size_t logicalCores = 0;
        std::unordered_set<std::string> physicalIds;
        
        while (std::getline(cpuInfoFile, line)) {
            if (line.find("processor") != std::string::npos) {
                ++logicalCores;
            } else if (line.find("physical id") != std::string::npos) {
                physicalIds.insert(line);
            }
        }
        
        physicalCores = physicalIds.size();
        metrics_.physical_core_usage = metrics_.cpu_usage * (static_cast<double>(physicalCores) / logicalCores);
        metrics_.logical_core_usage = metrics_.cpu_usage;
        
        std::ifstream memFile("/proc/meminfo");
        uint64_t totalMem = 0, freeMem = 0;
        while (std::getline(memFile, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            iss >> key >> value;
            
            if (key == "MemTotal:") totalMem = value;
            else if (key == "MemFree:") freeMem = value;
        }
        
        if (totalMem > 0) {
            metrics_.memory_usage = 1.0 - (static_cast<double>(freeMem) / totalMem);
        }
        
        std::ifstream powerFile("/sys/class/power_supply/BAT0/power_now");
        if (powerFile) {
            int power;
            powerFile >> power;
            metrics_.power_consumption = power / 1000000.0;
        }
        
        std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
        if (tempFile) {
            int temp;
            tempFile >> temp;
            metrics_.temperature = temp / 1000.0;
        }
        
        std::ifstream avxFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (avxFile) {
            int freq;
            avxFile >> freq;
            metrics_.avx_usage = freq > 2000000 ? 1.0 : 0.0;
        }
    }
    #endif

    void calculateEfficiency() {
        double efficiency = 0.0;
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            efficiency = (metrics_.performance_core_usage * 0.4 +
                         metrics_.efficiency_core_usage * 0.3 +
                         metrics_.neural_engine_usage * 0.3);
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            efficiency = (metrics_.physical_core_usage * 0.4 +
                         metrics_.logical_core_usage * 0.3 +
                         metrics_.avx_usage * 0.3);
        #endif
        
        metrics_.instructions_per_second = static_cast<uint64_t>(efficiency * 1000000000);
    }

    config::OptimizationConfig config_;
    metrics::PerformanceMetrics metrics_;
    mutable std::shared_mutex metricsMutex_;
};

class ResourceManager {
public:
    explicit ResourceManager(const config::ResourceLimits& config)
        : config_(config) {
        std::cout << "[DEBUG] ResourceManager ctor\n";
        initializeResources();
    }
    ~ResourceManager() {
        std::cout << "[DEBUG] ~ResourceManager ENTER\n";
        std::cout << "[DEBUG] ~ResourceManager EXIT\n";
    }

    bool allocateResource(const std::string& resource, double amount) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it == resources_.end()) return false;
        
        if (it->second.current + amount > it->second.limit) return false;
        
        it->second.current += amount;
        return true;
    }

    void deallocateResource(const std::string& resource, double amount) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it != resources_.end()) {
            it->second.current = std::max(0.0, it->second.current - amount);
        }
    }

    double getResourceEfficiency(const std::string& resource) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it == resources_.end()) return 0.0;
        
        return it->second.current / it->second.limit;
    }

private:
    void initializeResources() {
        resources_["cpu"] = {config_.maxCpuUsage, 0.0};
        resources_["memory"] = {static_cast<double>(config_.maxMemory), 0.0};
        resources_["power"] = {config_.maxPowerConsumption, 0.0};
        resources_["temperature"] = {config_.maxTemperature, 0.0};
    }

    struct Resource {
        double limit;
        double current;
    };

    config::ResourceLimits config_;
    std::unordered_map<std::string, Resource> resources_;
    mutable std::shared_mutex mutex_;
};

class AdaptiveController {
public:
    explicit AdaptiveController(const config::OptimizationConfig& config)
        : config_(config) {
        std::cout << "[DEBUG] AdaptiveController ctor\n";
        initializeController();
    }
    ~AdaptiveController() {
        std::cout << "[DEBUG] ~AdaptiveController ENTER\n";
    }

    void update(const metrics::PerformanceMetrics& metrics) {
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
        currentParameters_.resize(4); // CPU, Memory, Power, Temperature
        std::fill(currentParameters_.begin(), currentParameters_.end(), 0.5);
    }

    bool shouldAdapt() const {
        if (metricsHistory_.size() < 2) return false;
        const auto& current = metricsHistory_.back();
        const auto& previous = metricsHistory_[metricsHistory_.size() - 2];
        if (current.efficiencyScore < config_.minPerformanceThreshold) return true;
        double degradation = previous.efficiencyScore - current.efficiencyScore;
        if (degradation > 0.1) return true;
        return false;
    }

    void adapt() {
        double gradient = calculateGradient();
        for (auto& param : currentParameters_) {
            param = std::max(0.0, std::min(1.0, param - config_.learningRate * gradient));
        }
        // Убираем случайное исследование для упрощения
        // if (std::uniform_real_distribution<>(0, 1)(rng_) < config_.explorationRate) {
        //     explore();
        // }
    }

    double calculateGradient() const {
        if (metricsHistory_.size() < 2) return 0.0;
        const auto& current = metricsHistory_.back();
        const auto& previous = metricsHistory_[metricsHistory_.size() - 2];
        return (current.efficiencyScore - previous.efficiencyScore) /
               std::max(1e-6, std::abs(current.efficiencyScore - previous.efficiencyScore));
    }

    void explore() {
        // Упрощенное исследование без случайных чисел
        for (auto& param : currentParameters_) {
            param = std::max(0.0, std::min(1.0, param + 0.01));
        }
    }

    config::OptimizationConfig config_;
    std::deque<metrics::PerformanceMetrics> metricsHistory_;
    std::vector<double> currentParameters_;
    // Убираем std::mt19937 для упрощения
    // std::mt19937 rng_{std::random_device{}()};
    mutable std::mutex mutex_;
};

} // namespace detail

// Constructor and destructor implementation
CoreKernel::CoreKernel()
{
    std::cout << "[DEBUG] CoreKernel::CoreKernel() start\n";
    pImpl = std::make_unique<Impl>("");
    std::cout << "[DEBUG] CoreKernel::CoreKernel() after pImpl\n";
#ifndef COREKERNEL_DISABLE_LOGGER
    initializeLogger();
#endif
}

CoreKernel::CoreKernel(const std::string& id)
#ifndef DEBUG_COREKERNEL_NO_CACHE
    : dynamicCache(std::make_unique<cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>>>(128))
#endif
{
    std::cout << "[DEBUG] CoreKernel::CoreKernel(id) start\n";
    pImpl = std::make_unique<Impl>(id);
    std::cout << "[DEBUG] CoreKernel::CoreKernel(id) after pImpl\n";
#ifndef DEBUG_COREKERNEL_NO_COMPONENTS
    // Оставляем инициализацию других компонентов
    // (logger уже под своим препроцессором)
#endif
#ifndef COREKERNEL_DISABLE_LOGGER
    initializeLogger();
#endif
}

CoreKernel::~CoreKernel() {
    try {
        shutdown();
    } catch (const std::exception& e) {
        spdlog::error("Error during kernel shutdown: {}", e.what());
    }
}

// Move operations
CoreKernel::CoreKernel(CoreKernel&& other) noexcept
    : pImpl(std::move(other.pImpl)) {
}

CoreKernel& CoreKernel::operator=(CoreKernel&& other) noexcept {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

// Core functionality implementation
bool CoreKernel::initialize() {
    std::cout << "[DEBUG] CoreKernel::initialize() start\n";
    spdlog::info("CoreKernel::initialize() start");
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (pImpl->running) {
        spdlog::warn("CoreKernel[{}]: повторная инициализация", pImpl->id);
        return false;
    }
    try {
        spdlog::info("CoreKernel[{}]: initializeComponents start", pImpl->id);
        bool result = initializeComponents();
        std::cout << "[DEBUG] CoreKernel::initializeComponents() returned: " << result << "\n";
        spdlog::info("CoreKernel::initializeComponents() returned: {}", result);
        if (result) {
        pImpl->running = true;
            std::cout << "[DEBUG] CoreKernel::initialize() success\n";
            spdlog::info("CoreKernel::initialize() success");
        } else {
            std::cout << "[DEBUG] CoreKernel::initialize() failed\n";
            spdlog::error("CoreKernel::initialize() failed");
        }
        return result;
    } catch (const std::exception& e) {
        std::cout << "[DEBUG] CoreKernel::initialize() exception: " << e.what() << "\n";
        spdlog::error("CoreKernel::initialize() exception: {}", e.what());
        return false;
    } catch (...) {
        spdlog::error("CoreKernel[{}]: Неизвестная ошибка инициализации", pImpl->id);
        return false;
    }
}

void CoreKernel::shutdown() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (!pImpl->running) {
        spdlog::info("CoreKernel[{}]: shutdown() вызван повторно, ядро уже остановлено", pImpl->id);
        return;
    }
    spdlog::info("CoreKernel[{}]: shutdown() start", pImpl->id);
    bool error = false;
    std::string errorMsg;
    // Логируем каждый этап отдельно
    try {
        spdlog::debug("CoreKernel[{}]: shutdownComponents()...", pImpl->id);
    try {
        shutdownComponents();
    } catch (const std::exception& e) {
            spdlog::error("CoreKernel[{}]: shutdownComponents() exception: {}", pImpl->id, e.what());
            error = true;
            errorMsg = std::string("shutdownComponents: ") + e.what();
        }
        // Очистка dynamicCache
        if (dynamicCache) {
            spdlog::info("CoreKernel[{}]: очистка dynamicCache", pImpl->id);
            try {
                dynamicCache->clear();
                dynamicCache.reset();
            } catch (const std::exception& e) {
                spdlog::error("CoreKernel[{}]: dynamicCache exception: {}", pImpl->id, e.what());
                error = true;
                errorMsg = std::string("dynamicCache: ") + e.what();
            }
        }
        // Очистка recoveryManager
        if (recoveryManager) {
            spdlog::info("CoreKernel[{}]: завершение RecoveryManager", pImpl->id);
            try {
                recoveryManager->shutdown();
                recoveryManager.reset();
            } catch (const std::exception& e) {
                spdlog::error("CoreKernel[{}]: recoveryManager exception: {}", pImpl->id, e.what());
                error = true;
                errorMsg = std::string("recoveryManager: ") + e.what();
            }
        }
        // Очистка threadPool
        if (threadPool) {
            spdlog::info("CoreKernel[{}]: завершение ThreadPool", pImpl->id);
            try {
                threadPool->waitForCompletion();
                threadPool.reset();
            } catch (const std::exception& e) {
                spdlog::error("CoreKernel[{}]: threadPool exception: {}", pImpl->id, e.what());
                error = true;
                errorMsg = std::string("threadPool: ") + e.what();
            }
        }
        // Очистка platformOptimizer
        if (platformOptimizer) {
            spdlog::info("CoreKernel[{}]: завершение PlatformOptimizer", pImpl->id);
            try {
                platformOptimizer.reset();
            } catch (const std::exception& e) {
                spdlog::error("CoreKernel[{}]: platformOptimizer exception: {}", pImpl->id, e.what());
                error = true;
                errorMsg = std::string("platformOptimizer: ") + e.what();
            }
        }
        // УБИРАЕМ повторную очистку performanceMonitor, resourceManager, adaptiveController
        // так как они уже очищены в shutdownComponents()
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: shutdown() outer exception: {}", pImpl->id, e.what());
        error = true;
        errorMsg = std::string("outer: ") + e.what();
    }
    // Гарантируем сброс running даже при ошибках
    pImpl->running = false;
    if (error) {
        spdlog::error("CoreKernel[{}]: shutdown завершён с ошибкой: {}", pImpl->id, errorMsg);
    } else {
        spdlog::info("CoreKernel[{}]: shutdown завершён успешно", pImpl->id);
    }
}

bool CoreKernel::isRunning() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->running;
}

// Metrics implementation
metrics::PerformanceMetrics CoreKernel::getMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->currentMetrics;
}

void CoreKernel::updateMetrics() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->updateMetrics();
    std::vector<uint8_t> stateData{/* ... сериализация состояния ядра ... */};
    if (dynamicCache) dynamicCache->put("core_state", stateData);
}

// Resource management implementation
void CoreKernel::setResourceLimit(const std::string& resource, double limit) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->resourceLimits[resource] = limit;
}

double CoreKernel::getResourceUsage(const std::string& resource) const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    auto it = pImpl->resourceUsage.find(resource);
    return it != pImpl->resourceUsage.end() ? it->second : 0.0;
}

// Kernel information implementation
KernelType CoreKernel::getType() const {
    return KernelType::PARENT;
}

std::string CoreKernel::getId() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->id;
}

// Control implementation
void CoreKernel::pause() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->paused = true;
}

void CoreKernel::resume() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->paused = false;
}

void CoreKernel::reset() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    shutdownComponents();
    initializeComponents();
}

// Features implementation
std::vector<std::string> CoreKernel::getSupportedFeatures() const {
    std::vector<std::string> features;
    
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        features.push_back("neon");
        features.push_back("amx");
        features.push_back("metal");
        features.push_back("neural_engine");
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        features.push_back("avx2");
        features.push_back("avx512");
        features.push_back("perf_events");
    #endif
    
    return features;
}

// Child kernel management implementation
void CoreKernel::addChildKernel(std::shared_ptr<IKernel> kernel) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (!kernel) return;
    
    pImpl->childKernels[kernel->getId()] = kernel;
}

void CoreKernel::removeChildKernel(const std::string& kernelId) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->childKernels.erase(kernelId);
}

std::vector<std::shared_ptr<IKernel>> CoreKernel::getChildKernels() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    std::vector<std::shared_ptr<IKernel>> kernels;
    kernels.reserve(pImpl->childKernels.size());
    
    for (const auto& [id, kernel] : pImpl->childKernels) {
        kernels.push_back(kernel);
    }
    
    return kernels;
}

// Task management implementation
void CoreKernel::scheduleTask(std::function<void()> task, int priority) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (!pImpl->running) {
        spdlog::warn("CoreKernel[{}]: Попытка планирования задачи на остановленном ядре", pImpl->id);
        return;
    }
    
    std::string taskId = pImpl->generateUniqueId();
    pImpl->taskQueue.emplace(taskId, priority, std::move(task));
    pImpl->taskCondition.notify_one();
    spdlog::debug("CoreKernel[{}]: Задача {} запланирована с приоритетом {}", pImpl->id, taskId, priority);
}

void CoreKernel::cancelTask(const std::string& taskId) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    spdlog::info("CoreKernel: cancelling task id={}", taskId);
    pImpl->cancelledTasks.insert(taskId);
    
    // Создаем новую очередь без отмененных задач
    std::priority_queue<Impl::Task> newQueue;
    while (!pImpl->taskQueue.empty()) {
        auto task = pImpl->taskQueue.top();
        if (pImpl->cancelledTasks.count(task.id) == 0) {
            newQueue.push(std::move(task));
        } else {
            spdlog::debug("CoreKernel: удалена отменённая задача id={}", task.id);
        }
        pImpl->taskQueue.pop();
    }
    pImpl->taskQueue = std::move(newQueue);
}

// Architecture optimization implementation
void CoreKernel::optimizeForArchitecture() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    if (platformOptimizer) {
#ifdef CLOUD_PLATFORM_APPLE_ARM
        spdlog::info("CoreKernel[{}]: optimizeForArchitecture — ARM профиль", pImpl->id);
        core::cache::CacheConfig armConfig;
        armConfig.enableCompression = false;
        armConfig.enableMetrics = true;
        armConfig.maxSize = 1024 * 1024 * 128; // 128MB
        armConfig.storagePath = "./cache/arm";
        // Можно добавить специфичные параметры для NEON/AMX
        platformOptimizer->optimizeCache(armConfig);
#elif defined(CLOUD_PLATFORM_LINUX_X64)
        spdlog::info("CoreKernel[{}]: optimizeForArchitecture — Linux профиль", pImpl->id);
        core::cache::CacheConfig linuxConfig;
        linuxConfig.enableCompression = true;
        linuxConfig.enableMetrics = true;
        linuxConfig.maxSize = 1024 * 1024 * 256; // 256MB
        linuxConfig.storagePath = "./cache/linux";
        // Можно добавить специфичные параметры для AVX
        platformOptimizer->optimizeCache(linuxConfig);
#else
        spdlog::info("CoreKernel[{}]: optimizeForArchitecture — дефолтный профиль", pImpl->id);
        core::cache::CacheConfig defaultConfig;
        platformOptimizer->optimizeCache(defaultConfig);
#endif
    } else {
        spdlog::warn("CoreKernel[{}]: PlatformOptimizer не инициализирован", pImpl->id);
    }
}

void CoreKernel::enableHardwareAcceleration() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
#ifdef CLOUD_PLATFORM_APPLE_ARM
    spdlog::info("CoreKernel[{}]: enableHardwareAcceleration — Apple ARM", pImpl->id);
    if (platformOptimizer) {
        core::cache::CacheConfig armConfig;
        armConfig.enableCompression = false;
        armConfig.enableMetrics = true;
        armConfig.maxSize = 1024 * 1024 * 128; // 128MB
        armConfig.storagePath = "./cache/arm";
        // Можно добавить специфичные параметры для NEON/AMX
        platformOptimizer->optimizeCache(armConfig);
    }
#elif defined(CLOUD_PLATFORM_LINUX_X64)
    spdlog::info("CoreKernel[{}]: enableHardwareAcceleration — Linux X64", pImpl->id);
    if (platformOptimizer) {
        core::cache::CacheConfig linuxConfig;
        linuxConfig.enableCompression = true;
        linuxConfig.enableMetrics = true;
        linuxConfig.maxSize = 1024 * 1024 * 256; // 256MB
        linuxConfig.storagePath = "./cache/linux";
        // Можно добавить специфичные параметры для AVX
        platformOptimizer->optimizeCache(linuxConfig);
    }
#else
    spdlog::warn("CoreKernel[{}]: аппаратное ускорение не поддерживается на этой платформе", pImpl->id);
#endif
}

void CoreKernel::setPerformanceMode(bool highPerformance) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->highPerformanceMode = highPerformance;
    if (platformOptimizer) {
        if (highPerformance) {
            core::cache::CacheConfig highPerfConfig;
            highPerfConfig.enableCompression = false;
            highPerfConfig.enableMetrics = true;
            highPerfConfig.maxSize = 1024 * 1024 * 512; // 512MB
            highPerfConfig.storagePath = "./cache/high_perf";
            platformOptimizer->optimizeCache(highPerfConfig);
            spdlog::info("CoreKernel[{}]: включён high performance mode", pImpl->id);
        } else {
            core::cache::CacheConfig energySavingConfig;
            energySavingConfig.enableCompression = true;
            energySavingConfig.enableMetrics = true;
            energySavingConfig.maxSize = 1024 * 1024 * 64; // 64MB
            energySavingConfig.storagePath = "./cache/energy_saving";
            platformOptimizer->optimizeCache(energySavingConfig);
            spdlog::info("CoreKernel[{}]: включён energy saving mode", pImpl->id);
        }
        } else {
        spdlog::warn("CoreKernel[{}]: PlatformOptimizer не инициализирован", pImpl->id);
        }
}

bool CoreKernel::isHighPerformanceMode() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->highPerformanceMode;
}

// Event handling implementation
void CoreKernel::registerEventHandler(const std::string& event, EventCallback callback) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->eventHandlers[event].push_back(std::move(callback));
}

void CoreKernel::unregisterEventHandler(const std::string& event) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->eventHandlers.erase(event);
}

void CoreKernel::setPreloadManager(std::shared_ptr<core::PreloadManager> preloadManager) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    preloadManager_ = preloadManager;
    spdlog::info("CoreKernel[{}]: PreloadManager установлен", pImpl->id);
}

void CoreKernel::warmupFromPreload() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    if (!preloadManager_ || !dynamicCache) {
        spdlog::warn("CoreKernel[{}]: PreloadManager или DynamicCache недоступны для warm-up", pImpl->id);
        return;
    }
    
    try {
        spdlog::info("CoreKernel[{}]: Начинаем warm-up из PreloadManager", pImpl->id);
        
        // Получаем все ключи из PreloadManager
        auto keys = preloadManager_->getAllKeys();
        spdlog::debug("CoreKernel[{}]: Получено {} ключей для warm-up", pImpl->id, keys.size());
        
        // Получаем данные для ключей
        for (const auto& key : keys) {
            auto data = preloadManager_->getDataForKey(key);
            if (data) {
                dynamicCache->put(key, *data);
                spdlog::trace("CoreKernel[{}]: Загружен ключ '{}' в кэш", pImpl->id, key);
            }
        }
        
        spdlog::info("CoreKernel[{}]: Warm-up завершен, загружено {} элементов", pImpl->id, keys.size());
        notifyEvent("warmup_completed", keys.size());
        
            } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка при warm-up: {}", pImpl->id, e.what());
        notifyEvent("warmup_failed", std::string(e.what()));
    }
}

ExtendedKernelMetrics CoreKernel::getExtendedMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return extendedMetrics_;
}

void CoreKernel::updateExtendedMetrics() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    updateExtendedMetricsFromPerformance();
}

bool CoreKernel::processTask(const balancer::TaskDescriptor& task) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    
    if (!pImpl->running) {
        spdlog::warn("CoreKernel[{}]: Попытка обработки задачи на остановленном ядре", pImpl->id);
        return false;
    }
    
    try {
        spdlog::debug("CoreKernel[{}]: Обработка задачи типа {} с приоритетом {}", 
                     pImpl->id, static_cast<int>(task.type), task.priority);
        
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
        spdlog::debug("CoreKernel[{}]: Задача успешно обработана", pImpl->id);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка обработки задачи: {}", pImpl->id, e.what());
        notifyEvent("task_failed", std::string(e.what()));
        return false;
    }
}

void CoreKernel::scheduleTask(const balancer::TaskDescriptor& task) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    
    if (!pImpl->running) {
        spdlog::warn("CoreKernel[{}]: Попытка планирования задачи на остановленном ядре", pImpl->id);
        return;
    }
    // Используем конструктор Task
    pImpl->taskQueue.push(Impl::Task("", task.priority, [this, task]() {
        processTask(task);
    }));
    pImpl->taskCondition.notify_one();
    spdlog::debug("CoreKernel[{}]: Задача запланирована с приоритетом {}", pImpl->id, task.priority);
}

void CoreKernel::setTaskCallback(TaskCallback callback) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    taskCallback_ = callback;
    spdlog::debug("CoreKernel[{}]: TaskCallback установлен", pImpl->id);
}

void CoreKernel::setLoadBalancer(std::shared_ptr<balancer::LoadBalancer> loadBalancer) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    loadBalancer_ = loadBalancer;
    spdlog::info("CoreKernel[{}]: LoadBalancer установлен", pImpl->id);
}

std::shared_ptr<balancer::LoadBalancer> CoreKernel::getLoadBalancer() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return loadBalancer_;
}

void CoreKernel::setEventCallback(const std::string& event, EventCallback callback) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    eventCallbacks_[event] = callback;
    spdlog::debug("CoreKernel[{}]: EventCallback установлен для события '{}'", pImpl->id, event);
}

void CoreKernel::removeEventCallback(const std::string& event) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    eventCallbacks_.erase(event);
    spdlog::debug("CoreKernel[{}]: EventCallback удален для события '{}'", pImpl->id, event);
}

void CoreKernel::initializePreloadManager() {
    if (!preloadManager_) {
        spdlog::debug("CoreKernel[{}]: PreloadManager не установлен", pImpl->id);
        return;
    }
    
    try {
        if (preloadManager_->initialize()) {
            spdlog::info("CoreKernel[{}]: PreloadManager инициализирован", pImpl->id);
            warmupFromPreload();
        } else {
            spdlog::warn("CoreKernel[{}]: Не удалось инициализировать PreloadManager", pImpl->id);
        }
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка инициализации PreloadManager: {}", pImpl->id, e.what());
            }
        }

void CoreKernel::initializeLoadBalancer() {
    if (!loadBalancer_) {
        spdlog::debug("CoreKernel[{}]: LoadBalancer не установлен", pImpl->id);
        return;
    }
    
    try {
        spdlog::info("CoreKernel[{}]: LoadBalancer готов к работе", pImpl->id);
        notifyEvent("loadbalancer_ready", pImpl->id);
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка инициализации LoadBalancer: {}", pImpl->id, e.what());
    }
}

void CoreKernel::updateExtendedMetricsFromPerformance() {
    try {
        auto perfMetrics = getMetrics();
        
        // Основные метрики
        extendedMetrics_.load = perfMetrics.cpu_usage;
        extendedMetrics_.latency = 0.0; // latency отсутствует
        extendedMetrics_.cacheEfficiency = 0.0; // cacheEfficiency отсутствует
        extendedMetrics_.tunnelBandwidth = 0.0; // Значение по умолчанию, т.к. нет такого поля
        extendedMetrics_.activeTasks = pImpl->taskQueue.size();
        
        // Resource-Aware метрики
        extendedMetrics_.cpuUsage = perfMetrics.cpu_usage;
        extendedMetrics_.memoryUsage = perfMetrics.memory_usage;
        extendedMetrics_.networkBandwidth = 1000.0; // MB/s, можно получить из системных метрик
        extendedMetrics_.diskIO = 1000.0; // IOPS, можно получить из системных метрик
        extendedMetrics_.energyConsumption = perfMetrics.power_consumption;
        
        // Workload-Specific метрики (вычисляем на основе типа ядра и производительности)
        double baseEfficiency = perfMetrics.efficiencyScore;
        extendedMetrics_.cpuTaskEfficiency = baseEfficiency * (getType() == KernelType::COMPUTATIONAL ? 1.2 : 1.0);
        extendedMetrics_.ioTaskEfficiency = baseEfficiency * (getType() == KernelType::MICRO ? 1.1 : 1.0);
        extendedMetrics_.memoryTaskEfficiency = baseEfficiency * (getType() == KernelType::ARCHITECTURAL ? 1.15 : 1.0);
        extendedMetrics_.networkTaskEfficiency = baseEfficiency * (getType() == KernelType::ORCHESTRATION ? 1.25 : 1.0);
        
        spdlog::trace("CoreKernel[{}]: Расширенные метрики обновлены", pImpl->id);
        
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка обновления расширенных метрик: {}", pImpl->id, e.what());
    }
}

void CoreKernel::notifyEvent(const std::string& event, const std::any& data) {
    try {
        auto it = eventCallbacks_.find(event);
        if (it != eventCallbacks_.end()) {
            it->second(pImpl->id, data);
            spdlog::trace("CoreKernel[{}]: Событие '{}' обработано", pImpl->id, event);
        }
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка обработки события '{}': {}", pImpl->id, event, e.what());
    }
}

// Private implementation
void CoreKernel::initializeLogger() {
    try {
        auto logger = spdlog::get("kernel");
        if (!logger) {
            logger = spdlog::rotating_logger_mt("kernel", "logs/kernel.log", 1024 * 1024 * 5, 2);
        }
        logger->set_level(spdlog::level::debug);
        spdlog::set_default_logger(logger);
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
    }
}

bool CoreKernel::initializeComponents() {
    std::cout << "[DEBUG] performanceMonitor create\n";
    performanceMonitor = std::make_unique<detail::PerformanceMonitor>(config::OptimizationConfig{});
    std::cout << "[DEBUG] performanceMonitor OK\n";
    std::cout << "[DEBUG] resourceManager create\n";
    resourceManager = std::make_shared<detail::ResourceManager>(config::ResourceLimits{8, 1024*1024*100, 0.8, 100.0, 90.0});
    std::cout << "[DEBUG] resourceManager OK\n";
    // std::cout << "[DEBUG] adaptiveController create\n";
    // adaptiveController = std::make_shared<detail::AdaptiveController>(config::OptimizationConfig{});
    // std::cout << "[DEBUG] adaptiveController OK\n";
    return true;
}

void CoreKernel::shutdownComponents() {
    std::cout << "[DEBUG] shutdownComponents: ENTER\n";
    if (resourceManager) {
        std::cout << "[DEBUG] shutdownComponents: reset resourceManager use_count=" << resourceManager.use_count() << "\n";
    resourceManager.reset();
        std::cout << "[DEBUG] shutdownComponents: reset resourceManager OK\n";
    }
    if (performanceMonitor) {
        std::cout << "[DEBUG] shutdownComponents: reset performanceMonitor ptr=" << performanceMonitor.get() << "\n";
        performanceMonitor.reset();
        std::cout << "[DEBUG] shutdownComponents: reset performanceMonitor OK\n";
    }
    std::cout << "[DEBUG] shutdownComponents: done\n";
}

void CoreKernel::startWorkerThreads() {
    size_t numThreads = std::min(static_cast<size_t>(4), static_cast<size_t>(std::thread::hardware_concurrency()));
    pImpl->workerThreads.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
        pImpl->workerThreads.emplace_back([this] {
            spdlog::debug("CoreKernel[{}]: Worker thread started", pImpl->id);
            
            while (pImpl->running) {
                Impl::Task task("", 0, nullptr);
                bool hasTask = false;
                
                {
                    std::unique_lock<std::mutex> lock(pImpl->taskMutex);
                    pImpl->taskCondition.wait_for(lock, std::chrono::milliseconds(100), [this] {
                        return !pImpl->running || !pImpl->taskQueue.empty();
                    });
                    
                    if (!pImpl->running) {
                        spdlog::debug("CoreKernel[{}]: Worker thread stopping", pImpl->id);
                        break;
                    }
                    
                    if (!pImpl->taskQueue.empty()) {
                        task = std::move(pImpl->taskQueue.top());
                    pImpl->taskQueue.pop();
                        hasTask = true;
                    }
                }
                
                if (hasTask && !pImpl->cancelledTasks.count(task.id)) {
                try {
                        if (task.func) {
                            task.func();
                        }
                } catch (const std::exception& e) {
                        spdlog::error("CoreKernel[{}]: Error in worker thread: {}", pImpl->id, e.what());
                }
            }
            }
            
            spdlog::debug("CoreKernel[{}]: Worker thread stopped", pImpl->id);
        });
    }
    
    spdlog::info("CoreKernel[{}]: Started {} worker threads", pImpl->id, numThreads);
}

void CoreKernel::stopWorkerThreads() {
    pImpl->running = false;
    pImpl->taskCondition.notify_all();
    
    for (auto& thread : pImpl->workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    pImpl->workerThreads.clear();
}

#ifdef CLOUD_PLATFORM_APPLE_ARM
void CoreKernel::optimizeForAppleARM() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    if (platformOptimizer) {
        spdlog::info("CoreKernel[{}]: optimizeForAppleARM — PlatformOptimizer", pImpl->id);
        core::cache::CacheConfig armConfig;
        armConfig.enableCompression = false;
        armConfig.enableMetrics = true;
        armConfig.maxSize = 1024 * 1024 * 128; // 128MB
        armConfig.storagePath = "./cache/arm";
        // Можно добавить специфичные параметры для NEON/AMX
        platformOptimizer->optimizeCache(armConfig);
    } else {
        spdlog::warn("CoreKernel[{}]: PlatformOptimizer не инициализирован для Apple ARM", pImpl->id);
    }
}
#endif

#ifdef CLOUD_PLATFORM_LINUX_X64
void CoreKernel::optimizeForLinuxX64() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    if (platformOptimizer) {
        spdlog::info("CoreKernel[{}]: optimizeForLinuxX64 — PlatformOptimizer", pImpl->id);
        core::cache::CacheConfig linuxConfig;
        linuxConfig.enableCompression = true;
        linuxConfig.enableMetrics = true;
        linuxConfig.maxSize = 1024 * 1024 * 256; // 256MB
        linuxConfig.storagePath = "./cache/linux";
        // Можно добавить специфичные параметры для AVX
        platformOptimizer->optimizeCache(linuxConfig);
    } else {
        spdlog::warn("CoreKernel[{}]: PlatformOptimizer не инициализирован для Linux X64", pImpl->id);
    }
}

void CoreKernel::monitorLinuxX64Metrics() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    std::ifstream statFile("/proc/stat");
    std::string line;
    if (std::getline(statFile, line)) {
        std::istringstream iss(line);
        std::string cpu;
        uint64_t user, nice, system, idle, iowait, irq, softirq;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        uint64_t total = user + nice + system + idle + iowait + irq + softirq;
        uint64_t idleTotal = idle + iowait;
        pImpl->currentMetrics.cpu_usage = 1.0 - (static_cast<double>(idleTotal) / total);
    }
    std::ifstream memFile("/proc/meminfo");
    uint64_t totalMem = 0, freeMem = 0;
    while (std::getline(memFile, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        iss >> key >> value;
        if (key == "MemTotal:") totalMem = value;
        else if (key == "MemFree:") freeMem = value;
    }
    if (totalMem > 0) {
        pImpl->currentMetrics.memory_usage = 1.0 - (static_cast<double>(freeMem) / totalMem);
    }
    spdlog::debug("CoreKernel[{}]: monitorLinuxX64Metrics обновил cpu_usage={}, memory_usage={}", pImpl->id, pImpl->currentMetrics.cpu_usage, pImpl->currentMetrics.memory_usage);
}
#endif

} // namespace kernel
} // namespace core
} // namespace cloud

// Implementation of Impl methods
namespace cloud {
namespace core {
namespace kernel {

CoreKernel::Impl::Impl(const std::string& kernelId)
    : id(kernelId.empty() ? generateUniqueId() : kernelId)
    , paused(false)
    , highPerformanceMode(false)
    , lastOptimization(std::chrono::steady_clock::now())
    , running(false) {
    std::cout << "[DEBUG] CoreKernel::Impl::Impl(" << id << ")\n";
    initializeMetrics();
}

std::string CoreKernel::Impl::generateUniqueId() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();
    std::stringstream ss;
    ss << std::hex << value;
    return "kernel_" + ss.str();
}

void CoreKernel::Impl::initializeMetrics() {
    currentMetrics = metrics::PerformanceMetrics{
        .cpu_usage = 0.0,
        .memory_usage = 0.0,
        .power_consumption = 0.0,
        .temperature = 0.0,
        .instructions_per_second = 0,
        .timestamp = std::chrono::steady_clock::now()
    };
}

void CoreKernel::Impl::updateMetrics() {
    // Platform-specific metrics update
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        updateAppleMetrics();
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        updateLinuxMetrics();
    #endif
    
    currentMetrics.timestamp = std::chrono::steady_clock::now();
}

#ifdef CLOUD_PLATFORM_APPLE_ARM
void CoreKernel::Impl::updateAppleMetrics() {
    // Get CPU usage
    processor_cpu_load_info_t cpuLoad;
    mach_msg_type_number_t count;
    host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                       reinterpret_cast<processor_info_t*>(&cpuLoad), &count);
    
    double totalUsage = 0.0;
    for (size_t i = 0; i < count; ++i) {
        totalUsage += (cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                      cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM]) /
                     static_cast<double>(cpuLoad[i].cpu_ticks[CPU_STATE_IDLE]);
    }
    currentMetrics.cpu_usage = totalUsage / count;
    
    // Get memory usage
    vm_size_t pageSize;
    host_page_size(mach_host_self(), &pageSize);
    
    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);
    host_statistics64(mach_host_self(), HOST_VM_INFO64,
                     reinterpret_cast<host_info64_t>(&vmStats), &infoCount);
    
    uint64_t totalMemory = vmStats.free_count + vmStats.active_count +
                          vmStats.inactive_count + vmStats.wire_count;
    currentMetrics.memory_usage = 1.0 - (static_cast<double>(vmStats.free_count) / totalMemory);
    
    // Get power consumption (approximate)
    int power;
    size_t size = sizeof(power);
    if (sysctlbyname("machdep.cpu.power", &power, &size, nullptr, 0) == 0) {
        currentMetrics.power_consumption = power / 1000.0;
    }
    
    // Get temperature
    int temp;
    size = sizeof(temp);
    if (sysctlbyname("machdep.cpu.temperature", &temp, &size, nullptr, 0) == 0) {
        currentMetrics.temperature = temp / 100.0;
    }
}
#elif defined(CLOUD_PLATFORM_LINUX_X64)
void CoreKernel::Impl::updateLinuxMetrics() {
    // Get CPU usage from /proc/stat
    std::ifstream statFile("/proc/stat");
    std::string line;
    if (std::getline(statFile, line)) {
        std::istringstream iss(line);
        std::string cpu;
        uint64_t user, nice, system, idle, iowait, irq, softirq;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        
        uint64_t total = user + nice + system + idle + iowait + irq + softirq;
        uint64_t idleTotal = idle + iowait;
        
        currentMetrics.cpu_usage = 1.0 - (static_cast<double>(idleTotal) / total);
    }
    
    // Get memory usage from /proc/meminfo
    std::ifstream memFile("/proc/meminfo");
    uint64_t totalMem = 0, freeMem = 0;
    while (std::getline(memFile, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        iss >> key >> value;
        
        if (key == "MemTotal:") totalMem = value;
        else if (key == "MemFree:") freeMem = value;
    }
    
    if (totalMem > 0) {
        currentMetrics.memory_usage = 1.0 - (static_cast<double>(freeMem) / totalMem);
    }
    
    // Get power consumption (if available)
    std::ifstream powerFile("/sys/class/power_supply/BAT0/power_now");
    if (powerFile) {
        int power;
        powerFile >> power;
        currentMetrics.power_consumption = power / 1000000.0;
    }
    
    // Get temperature (if available)
    std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (tempFile) {
        int temp;
        tempFile >> temp;
        currentMetrics.temperature = temp / 1000.0;
    }
}
#endif

} // namespace kernel
} // namespace core
} // namespace cloud
