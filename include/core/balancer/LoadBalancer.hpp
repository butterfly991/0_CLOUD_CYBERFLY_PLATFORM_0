#pragma once
#include <vector>
#include <memory>
#include <string>
#include <mutex>
namespace cloud { namespace core { namespace kernel { class IKernel; }}}
#include <chrono>
#include "core/balancer/TaskTypes.hpp"
#include <cstddef>

namespace cloud {
namespace core {
namespace balancer {

// Стратегии балансировки
// Определяет различные алгоритмы балансировки нагрузки между ядрами.
enum class BalancingStrategy {
    ResourceAware,      // Балансировка по ресурсам
    WorkloadSpecific,   // Балансировка по типу нагрузки
    HybridAdaptive,     // Гибридная адаптивная стратегия
    PriorityAdaptive,   // Приоритетная (устаревшая)
    LeastLoaded,        // Наименее загруженное ядро
    RoundRobin          // Round-robin
};

// LoadBalancer — гибридная Resource-Aware + Workload-Specific балансировка
// Продвинутый балансировщик нагрузки, который комбинирует Resource-Aware и Workload-Specific
// подходы для оптимального распределения задач между ядрами. Поддерживает адаптивное
// переключение стратегий на основе состояния системы.
// Потокобезопасен
// Поддерживает детальное логирование и метрики
class LoadBalancer {
public:
    LoadBalancer();
    
    // Деструктор
    // Освобождает ресурсы балансировщика.
    ~LoadBalancer();

    // Гибридная Resource-Aware + Workload-Specific балансировка
    // Распределяет задачи между ядрами, используя комбинированный подход:
    // - Resource-Aware: учитывает доступность ресурсов (CPU, память, сеть, энергия)
    // - Workload-Specific: учитывает эффективность ядра для конкретного типа задачи
    // - Адаптивное переключение: автоматически переключается между стратегиями
    // kernels: Вектор доступных ядер
    // tasks: Вектор задач для распределения
    // metrics: Метрики ядер (должен соответствовать размеру kernels)
    // Задачи обрабатываются по приоритету (высокий приоритет первым)
    // Метрики обновляются в реальном времени
    void balance(const std::vector<std::shared_ptr<cloud::core::kernel::IKernel>>& kernels,
                 const std::vector<TaskDescriptor>& tasks,
                 const std::vector<KernelMetrics>& metrics); // Балансировка задач

    // Установить веса для Resource-Aware метрик
    // Позволяет настроить важность различных ресурсов при принятии решений.
    // Сумма весов должна быть равна 1.0.
    // cpuWeight: Вес CPU (0.0-1.0)
    // memoryWeight: Вес памяти (0.0-1.0)
    // networkWeight: Вес сети (0.0-1.0)
    // energyWeight: Вес энергии (0.0-1.0)
    // Веса автоматически нормализуются
    void setResourceWeights(double cpuWeight, double memoryWeight, 
                           double networkWeight, double energyWeight); // Веса ресурсов
    
    // Установить пороги для адаптивного переключения стратегий
    // Определяет условия для автоматического переключения между Resource-Aware
    // и Workload-Specific стратегиями.
    // resourceThreshold: Порог ресурсов для переключения на Resource-Aware
    // workloadThreshold: Порог эффективности для переключения на Workload-Specific
    void setAdaptiveThresholds(double resourceThreshold, double workloadThreshold); // Пороги адаптации

    // Старые методы для обратной совместимости
    // Балансировка между ядрами (устаревший метод)
    // Используйте balance(kernels, tasks, metrics)
    void balance(const std::vector<std::shared_ptr<cloud::core::kernel::IKernel>>& kernels);
    
    // Балансировка задач между очередями (устаревший метод)
    // Используйте balance(kernels, tasks, metrics)
    void balanceTasks(std::vector<std::vector<uint8_t>>& taskQueues);

    // Установить стратегию балансировки по строке
    // strategy: Название стратегии ("resource_aware", "workload_specific", "hybrid_adaptive", etc.)
    void setStrategy(const std::string& strategy); // Установить стратегию
    
    // Получить текущую стратегию балансировки
    // return: Название текущей стратегии
    std::string getStrategy() const; // Получить стратегию
    
    // Установить стратегию балансировки по enum
    // s: Стратегия балансировки
    void setStrategy(BalancingStrategy s); // Установить стратегию (enum)
    
    // Получить текущую стратегию балансировки как enum
    // return: Текущая стратегия балансировки
    BalancingStrategy getStrategyEnum() const; // Получить стратегию (enum)

private:
    // Resource-Aware методы
    // Выбор ядра по Resource-Aware стратегии
    // Выбирает ядро на основе доступности ресурсов и предполагаемого использования.
    // metrics: Метрики всех ядер
    // task: Задача для распределения
    // return: Индекс выбранного ядра
    size_t selectByResourceAware(const std::vector<KernelMetrics>& metrics, 
                                const TaskDescriptor& task);
    
    // Workload-Specific методы
    // Выбор ядра по Workload-Specific стратегии
    // Выбирает ядро на основе эффективности для конкретного типа задачи.
    // metrics: Метрики всех ядер
    // task: Задача для распределения
    // return: Индекс выбранного ядра
    size_t selectByWorkloadSpecific(const std::vector<KernelMetrics>& metrics,
                                   const TaskDescriptor& task);
    
    // Гибридные методы
    // Выбор ядра по гибридной адаптивной стратегии
    // Комбинирует Resource-Aware и Workload-Specific подходы с адаптивным переключением.
    // metrics: Метрики всех ядер
    // task: Задача для распределения
    // return: Индекс выбранного ядра
    size_t selectByHybridAdaptive(const std::vector<KernelMetrics>& metrics,
                                 const TaskDescriptor& task);
    
    // Вспомогательные методы
    // Вычислить Resource-Aware score для ядра
    // metrics: Метрики ядра
    // task: Задача
    // return: Score (меньше = лучше)
    double calculateResourceScore(const KernelMetrics& metrics, 
                                 const TaskDescriptor& task);
    
    // Вычислить Workload-Specific score для ядра
    // metrics: Метрики ядра
    // task: Задача
    // return: Score (меньше = лучше)
    double calculateWorkloadScore(const KernelMetrics& metrics,
                                 const TaskDescriptor& task);
    
    // Определить необходимость переключения стратегии
    // metrics: Метрики всех ядер
    // return: true если нужно переключить стратегию
    bool shouldSwitchStrategy(const std::vector<KernelMetrics>& metrics);
    
    std::string strategy; // Текущая стратегия (строка)
    BalancingStrategy strategyEnum = BalancingStrategy::HybridAdaptive; // Текущая стратегия (enum)
    mutable std::mutex mutex_; // Мьютекс
    size_t rrIdx = 0; // Индекс для round-robin
    
    // Resource-Aware веса
    double cpuWeight_ = 0.3; // Вес CPU
    double memoryWeight_ = 0.25; // Вес памяти
    double networkWeight_ = 0.25; // Вес сети
    double energyWeight_ = 0.2; // Вес энергии
    
    // Адаптивные пороги
    double resourceThreshold_ = 0.8; // Порог ресурсов
    double workloadThreshold_ = 0.7; // Порог эффективности
    
    // Статистика для адаптации
    size_t resourceAwareDecisions_ = 0; // Счётчик решений Resource-Aware
    size_t workloadSpecificDecisions_ = 0; // Счётчик решений Workload-Specific
    size_t totalDecisions_ = 0; // Всего решений
};

} // namespace balancer
} // namespace core
} // namespace cloud
