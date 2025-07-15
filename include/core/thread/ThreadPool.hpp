#pragma once

#include <vector>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

// Определение платформо-зависимых макросов
#if defined(__APPLE__) && defined(__arm64__)
    #define CLOUD_PLATFORM_APPLE_ARM
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#elif defined(__linux__) && defined(__x86_64__)
    #define CLOUD_PLATFORM_LINUX_X64
    #include <pthread.h>
#endif

namespace cloud {
namespace core {
namespace thread {

// Структура для хранения метрик пула потоков
struct ThreadPoolMetrics {
    size_t activeThreads;    // Активные потоки
    size_t queueSize;        // Размер очереди
    size_t totalThreads;     // Всего потоков
};

// Структура для конфигурации пула потоков
struct ThreadPoolConfig {
    size_t minThreads;       // Мин. потоки
    size_t maxThreads;       // Макс. потоки
    size_t queueSize;        // Макс. очередь
    size_t stackSize;        // Размер стека
    
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        bool usePerformanceCores;    // Использовать производительные ядра
        bool useEfficiencyCores;     // Использовать энергоэффективные ядра
        size_t performanceCoreCount; // Кол-во производительных ядер
        size_t efficiencyCoreCount;  // Кол-во энергоэффективных ядер
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        bool useHyperthreading;      // Гипертрейдинг
        size_t physicalCoreCount;    // Физические ядра
        size_t logicalCoreCount;     // Логические ядра
    #endif
    
    bool validate() const {
        if (minThreads > maxThreads) return false;
        if (minThreads == 0) return false;
        if (stackSize == 0) return false;
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            if (usePerformanceCores && performanceCoreCount == 0) return false;
            if (useEfficiencyCores && efficiencyCoreCount == 0) return false;
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            if (useHyperthreading && logicalCoreCount <= physicalCoreCount) return false;
        #endif
        
        return true;
    }
};

// Пул потоков
class ThreadPool {
public:
    explicit ThreadPool(const ThreadPoolConfig& config); // Конструктор
    ~ThreadPool(); // Деструктор
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    void enqueue(std::function<void()> task); // Добавить задачу
    size_t getActiveThreadCount() const; // Активные потоки
    size_t getQueueSize() const; // Размер очереди
    bool isQueueEmpty() const; // Очередь пуста?
    void waitForCompletion(); // Ждать завершения
    void stop(); // Остановить пул
    void restart(); // Перезапустить пул
    ThreadPoolMetrics getMetrics() const; // Метрики
    void updateMetrics(); // Обновить метрики
    void setConfiguration(const ThreadPoolConfig& config); // Установить конфиг
    ThreadPoolConfig getConfiguration() const; // Получить конфиг
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl; // Реализация
};

} // namespace thread
} // namespace core
} // namespace cloud