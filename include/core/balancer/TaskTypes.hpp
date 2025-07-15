#pragma once
#include <vector>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cloud { namespace core { namespace kernel { class IKernel; } } }

namespace cloud {
namespace core {
namespace balancer {

// Типы задач для Workload-Specific балансировки
enum class TaskType {
    CPU_INTENSIVE,
    IO_INTENSIVE,
    MEMORY_INTENSIVE,
    NETWORK_INTENSIVE,
    MIXED
};

// Расширенные метрики ядра для Resource-Aware балансировки
struct KernelMetrics {
    double cpuUsage = 0.0;
    double memoryUsage = 0.0;
    double networkBandwidth = 0.0;
    double diskIO = 0.0;
    double energyConsumption = 0.0;
    double cpuTaskEfficiency = 0.0;
    double ioTaskEfficiency = 0.0;
    double memoryTaskEfficiency = 0.0;
    double networkTaskEfficiency = 0.0;
};

// Расширенный дескриптор задачи
struct TaskDescriptor {
    std::vector<uint8_t> data;
    int priority = 5;
    std::chrono::steady_clock::time_point enqueueTime;
    TaskType type = TaskType::MIXED;
    size_t estimatedMemoryUsage = 0;
    size_t estimatedCpuTime = 0;
    TaskDescriptor() = default;
};

} // namespace balancer
} // namespace core
} // namespace cloud