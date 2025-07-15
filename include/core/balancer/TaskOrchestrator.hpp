#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace balancer {

// TaskOrchestrator — управление очередями задач, расширенные стратегии
class TaskOrchestrator {
public:
    TaskOrchestrator();
    ~TaskOrchestrator();
    void enqueueTask(const std::vector<uint8_t>& data); // Добавить задачу
    bool dequeueTask(std::vector<uint8_t>& data);       // Извлечь задачу
    size_t queueSize() const;                           // Размер очереди
    void setOrchestrationPolicy(const std::string& policy); // Стратегия оркестрации
    std::string getOrchestrationPolicy() const;         // Получить стратегию
private:
    std::vector<std::vector<uint8_t>> taskQueue; // Очередь задач
    std::string orchestrationPolicy; // Стратегия
    mutable std::mutex mutex_;
};

} // namespace balancer
} // namespace core
} // namespace cloud
