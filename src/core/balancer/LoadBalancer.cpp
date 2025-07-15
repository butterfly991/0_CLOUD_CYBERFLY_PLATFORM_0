#include "core/balancer/LoadBalancer.hpp"
#include "core/balancer/TaskTypes.hpp"
#include <spdlog/spdlog.h>
#include <numeric>
#include <algorithm>
#include "core/kernel/base/CoreKernel.hpp"

namespace cloud {
namespace core {
namespace balancer {

LoadBalancer::LoadBalancer() : strategy("hybrid_adaptive") {}
LoadBalancer::~LoadBalancer() {}

void LoadBalancer::balance(const std::vector<std::shared_ptr<cloud::core::kernel::IKernel>>& kernels,
                           const std::vector<TaskDescriptor>& tasks,
                           const std::vector<KernelMetrics>& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (kernels.empty() || tasks.empty() || metrics.size() != kernels.size()) return;
    
    spdlog::info("[LB] Гибридная балансировка: {} задач на {} ядер", tasks.size(), kernels.size());
    
    // Адаптивное переключение стратегий
    if (shouldSwitchStrategy(metrics)) {
        if (strategyEnum == BalancingStrategy::ResourceAware) {
            strategyEnum = BalancingStrategy::WorkloadSpecific;
            strategy = "workload_specific";
            spdlog::info("[LB] Переключение на Workload-Specific стратегию");
        } else {
            strategyEnum = BalancingStrategy::ResourceAware;
            strategy = "resource_aware";
            spdlog::info("[LB] Переключение на Resource-Aware стратегию");
        }
    }
    
    // Распределение задач по приоритету
    std::vector<TaskDescriptor> highPriority, lowPriority;
    for (const auto& task : tasks) {
        if (task.priority >= 7) highPriority.push_back(task);
        else lowPriority.push_back(task);
    }
    
    // Обработка высокоприоритетных задач
    for (const auto& task : highPriority) {
        size_t selectedKernel = 0;
        switch (strategyEnum) {
            case BalancingStrategy::ResourceAware:
                selectedKernel = selectByResourceAware(metrics, task);
                ++resourceAwareDecisions_;
                break;
            case BalancingStrategy::WorkloadSpecific:
                selectedKernel = selectByWorkloadSpecific(metrics, task);
                ++workloadSpecificDecisions_;
                break;
            case BalancingStrategy::HybridAdaptive:
                selectedKernel = selectByHybridAdaptive(metrics, task);
                break;
            default:
                selectedKernel = selectByResourceAware(metrics, task);
                break;
        }
        ++totalDecisions_;
        
        kernels[selectedKernel]->scheduleTask([data = task.data]() {/* обработка */;}, task.priority);
        spdlog::info("[LB] High-priority {} task отправлен на ядро {} (стратегия: {})", 
                    static_cast<int>(task.type), selectedKernel, strategy);
    }
    
    // Обработка низкоприоритетных задач
    for (const auto& task : lowPriority) {
        size_t selectedKernel = 0;
        switch (strategyEnum) {
            case BalancingStrategy::ResourceAware:
                selectedKernel = selectByResourceAware(metrics, task);
                ++resourceAwareDecisions_;
                break;
            case BalancingStrategy::WorkloadSpecific:
                selectedKernel = selectByWorkloadSpecific(metrics, task);
                ++workloadSpecificDecisions_;
                break;
            case BalancingStrategy::HybridAdaptive:
                selectedKernel = selectByHybridAdaptive(metrics, task);
                break;
            default:
                selectedKernel = selectByResourceAware(metrics, task);
                break;
        }
        ++totalDecisions_;
        
        kernels[selectedKernel]->scheduleTask([data = task.data]() {/* обработка */;}, task.priority);
        spdlog::debug("[LB] Low-priority {} task отправлен на ядро {} (стратегия: {})", 
                     static_cast<int>(task.type), selectedKernel, strategy);
    }
    
    // Логирование статистики
    if (totalDecisions_ > 0) {
        double resourceRatio = static_cast<double>(resourceAwareDecisions_) / totalDecisions_;
        double workloadRatio = static_cast<double>(workloadSpecificDecisions_) / totalDecisions_;
        spdlog::info("[LB] Статистика решений: Resource-Aware={:.1f}%, Workload-Specific={:.1f}%", 
                    resourceRatio * 100.0, workloadRatio * 100.0);
    }
}

size_t LoadBalancer::selectByResourceAware(const std::vector<KernelMetrics>& metrics, 
                                          const TaskDescriptor& task) {
    double bestScore = std::numeric_limits<double>::max();
    size_t bestKernel = 0;
    
    // Если все метрики одинаковые, используем round-robin
    bool allEqual = true;
    double firstScore = calculateResourceScore(metrics[0], task);
    
    for (size_t i = 1; i < metrics.size(); ++i) {
        double score = calculateResourceScore(metrics[i], task);
        if (std::abs(score - firstScore) > 0.001) {
            allEqual = false;
            break;
        }
    }
    
    if (allEqual) {
        bestKernel = rrIdx % metrics.size();
        rrIdx = (rrIdx + 1) % metrics.size();
        spdlog::debug("[LB] Resource-Aware: все метрики равны, используем round-robin: ядро {}", bestKernel);
        return bestKernel;
    }
    
    for (size_t i = 0; i < metrics.size(); ++i) {
        double score = calculateResourceScore(metrics[i], task);
        if (score < bestScore) {
            bestScore = score;
            bestKernel = i;
        }
    }
    
    spdlog::debug("[LB] Resource-Aware: выбрано ядро {} (score={:.3f})", bestKernel, bestScore);
    return bestKernel;
}

size_t LoadBalancer::selectByWorkloadSpecific(const std::vector<KernelMetrics>& metrics,
                                             const TaskDescriptor& task) {
    double bestScore = std::numeric_limits<double>::max();
    size_t bestKernel = 0;
    
    for (size_t i = 0; i < metrics.size(); ++i) {
        double score = calculateWorkloadScore(metrics[i], task);
            if (score < bestScore) {
                bestScore = score;
            bestKernel = i;
        }
    }
    
    spdlog::debug("[LB] Workload-Specific: выбрано ядро {} (score={:.3f}) для типа {}", 
                 bestKernel, bestScore, static_cast<int>(task.type));
    return bestKernel;
}

size_t LoadBalancer::selectByHybridAdaptive(const std::vector<KernelMetrics>& metrics,
                                           const TaskDescriptor& task) {
    // Гибридный подход: комбинируем Resource-Aware и Workload-Specific
    double bestScore = std::numeric_limits<double>::max();
    size_t bestKernel = 0;
    
    // Если все метрики одинаковые, используем round-robin
    bool allEqual = true;
    double firstResourceScore = calculateResourceScore(metrics[0], task);
    double firstWorkloadScore = calculateWorkloadScore(metrics[0], task);
    
    for (size_t i = 1; i < metrics.size(); ++i) {
        double resourceScore = calculateResourceScore(metrics[i], task);
        double workloadScore = calculateWorkloadScore(metrics[i], task);
        if (std::abs(resourceScore - firstResourceScore) > 0.001 || 
            std::abs(workloadScore - firstWorkloadScore) > 0.001) {
            allEqual = false;
            break;
        }
    }
    
    if (allEqual) {
        bestKernel = rrIdx % metrics.size();
        rrIdx = (rrIdx + 1) % metrics.size();
        spdlog::debug("[LB] Hybrid: все метрики равны, используем round-robin: ядро {}", bestKernel);
        return bestKernel;
    }
    
    for (size_t i = 0; i < metrics.size(); ++i) {
        double resourceScore = calculateResourceScore(metrics[i], task);
        double workloadScore = calculateWorkloadScore(metrics[i], task);
        
        // Адаптивные веса в зависимости от типа задачи
        double resourceWeight = 0.6;
        double workloadWeight = 0.4;
        
        // Для специфических типов задач увеличиваем вес workload
        if (task.type != TaskType::MIXED) {
            workloadWeight = 0.7;
            resourceWeight = 0.3;
        }
        
        // Если ресурсы критичны, увеличиваем вес resource
        if (resourceScore > resourceThreshold_) {
            resourceWeight = 0.8;
            workloadWeight = 0.2;
        }
        
        // Комбинированный score
        double combinedScore = resourceWeight * resourceScore + workloadWeight * workloadScore;
        
        if (combinedScore < bestScore) {
            bestScore = combinedScore;
            bestKernel = i;
        }
    }
    
    spdlog::debug("[LB] Hybrid: выбрано ядро {} (score={:.3f}, type={})", 
                 bestKernel, bestScore, static_cast<int>(task.type));
    return bestKernel;
}

double LoadBalancer::calculateResourceScore(const KernelMetrics& metrics, 
                                           const TaskDescriptor& task) {
    // Resource-Aware scoring: учитываем доступность ресурсов
    double cpuScore = (1.0 - metrics.cpuUsage) * cpuWeight_;
    double memoryScore = (1.0 - metrics.memoryUsage) * memoryWeight_;
    double networkScore = (metrics.networkBandwidth / 1000.0) * networkWeight_; // нормализуем к 1GB/s
    double energyScore = (1.0 - metrics.energyConsumption / 100.0) * energyWeight_; // нормализуем к 100W
    
    // Учитываем предполагаемое использование ресурсов задачей
    if (task.estimatedMemoryUsage > 0) {
        memoryScore *= (1.0 - task.estimatedMemoryUsage / (1024 * 1024 * 1024.0)); // нормализуем к 1GB
    }
    
    return cpuScore + memoryScore + networkScore + energyScore;
}

double LoadBalancer::calculateWorkloadScore(const KernelMetrics& metrics,
                                           const TaskDescriptor& task) {
    // Workload-Specific scoring: учитываем эффективность для типа задачи
    double efficiencyScore = 0.0;
    
    switch (task.type) {
        case TaskType::CPU_INTENSIVE:
            efficiencyScore = metrics.cpuTaskEfficiency;
            // Дополнительно учитываем загрузку CPU (меньше загрузка = лучше)
            efficiencyScore *= (1.0 - metrics.cpuUsage * 0.3);
            break;
        case TaskType::IO_INTENSIVE:
            efficiencyScore = metrics.ioTaskEfficiency;
            // Дополнительно учитываем дисковую активность
            efficiencyScore *= (1.0 + metrics.diskIO / 1000.0 * 0.1);
            break;
        case TaskType::MEMORY_INTENSIVE:
            efficiencyScore = metrics.memoryTaskEfficiency;
            // Дополнительно учитываем загрузку памяти
            efficiencyScore *= (1.0 - metrics.memoryUsage * 0.3);
            break;
        case TaskType::NETWORK_INTENSIVE:
            efficiencyScore = metrics.networkTaskEfficiency;
            // Дополнительно учитываем сетевую пропускную способность
            efficiencyScore *= (1.0 + metrics.networkBandwidth / 1000.0 * 0.1);
            break;
        case TaskType::MIXED:
            efficiencyScore = (metrics.cpuTaskEfficiency + metrics.ioTaskEfficiency + 
                              metrics.memoryTaskEfficiency + metrics.networkTaskEfficiency) / 4.0;
            break;
    }
    
    // Нормализуем к диапазону [0, 1]
    efficiencyScore = std::max(0.0, std::min(1.0, efficiencyScore));
    
    // Инвертируем score (меньше = лучше)
    return 1.0 - efficiencyScore;
}

bool LoadBalancer::shouldSwitchStrategy(const std::vector<KernelMetrics>& metrics) {
    // Адаптивное переключение на основе состояния ресурсов
    double avgCpuUsage = 0.0, avgMemoryUsage = 0.0;
    
    for (const auto& m : metrics) {
        avgCpuUsage += m.cpuUsage;
        avgMemoryUsage += m.memoryUsage;
    }
    avgCpuUsage /= metrics.size();
    avgMemoryUsage /= metrics.size();
    
    // Переключаемся если ресурсы критичны
    return (avgCpuUsage > 0.9 || avgMemoryUsage > 0.9);
}

void LoadBalancer::setResourceWeights(double cpuWeight, double memoryWeight, 
                                     double networkWeight, double energyWeight) {
    std::lock_guard<std::mutex> lock(mutex_);
    cpuWeight_ = cpuWeight;
    memoryWeight_ = memoryWeight;
    networkWeight_ = networkWeight;
    energyWeight_ = energyWeight;
    spdlog::info("[LB] Resource weights updated: CPU={}, Memory={}, Network={}, Energy={}", 
                cpuWeight_, memoryWeight_, networkWeight_, energyWeight_);
}

void LoadBalancer::setAdaptiveThresholds(double resourceThreshold, double workloadThreshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    resourceThreshold_ = resourceThreshold;
    workloadThreshold_ = workloadThreshold;
    spdlog::info("[LB] Adaptive thresholds updated: Resource={}, Workload={}", 
                resourceThreshold_, workloadThreshold_);
}

// Старые методы для обратной совместимости
void LoadBalancer::balance(const std::vector<std::shared_ptr<cloud::core::kernel::IKernel>>& kernels) {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::debug("LoadBalancer: балансировка между {} ядрами (стратегия: {})", kernels.size(), strategy);
}

void LoadBalancer::balanceTasks(std::vector<std::vector<uint8_t>>& taskQueues) {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::debug("LoadBalancer: балансировка задач между {} очередями (стратегия: {})", taskQueues.size(), strategy);
}

void LoadBalancer::setStrategy(const std::string& s) {
    std::lock_guard<std::mutex> lock(mutex_);
    strategy = s;
    if (s == "resource_aware") strategyEnum = BalancingStrategy::ResourceAware;
    else if (s == "workload_specific") strategyEnum = BalancingStrategy::WorkloadSpecific;
    else if (s == "hybrid_adaptive") strategyEnum = BalancingStrategy::HybridAdaptive;
    else if (s == "least_loaded") strategyEnum = BalancingStrategy::LeastLoaded;
    else if (s == "round_robin") strategyEnum = BalancingStrategy::RoundRobin;
    else strategyEnum = BalancingStrategy::PriorityAdaptive;
    spdlog::debug("LoadBalancer: установлена стратегия '{}', enum {}", s, static_cast<int>(strategyEnum));
}

std::string LoadBalancer::getStrategy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return strategy;
}

void LoadBalancer::setStrategy(BalancingStrategy s) {
    std::lock_guard<std::mutex> lock(mutex_);
    strategyEnum = s;
    if (s == BalancingStrategy::ResourceAware) strategy = "resource_aware";
    else if (s == BalancingStrategy::WorkloadSpecific) strategy = "workload_specific";
    else if (s == BalancingStrategy::HybridAdaptive) strategy = "hybrid_adaptive";
    else if (s == BalancingStrategy::LeastLoaded) strategy = "least_loaded";
    else if (s == BalancingStrategy::RoundRobin) strategy = "round_robin";
    else strategy = "priority_adaptive";
}

BalancingStrategy LoadBalancer::getStrategyEnum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return strategyEnum;
}

} // namespace balancer
} // namespace core
} // namespace cloud
