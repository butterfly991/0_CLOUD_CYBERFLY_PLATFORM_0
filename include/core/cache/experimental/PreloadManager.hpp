#pragma once

#include <unordered_set>
#include <initializer_list>
#include <functional>
#include <memory>
#include <atomic>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "core/thread/ThreadPool.hpp"

namespace cloud {
namespace core {
namespace cache {
namespace experimental {

// PreloadConfig — параметры для настройки предзагрузки (очередь, batch, окно, порог)
struct PreloadConfig {
    size_t maxQueueSize; // Макс. очередь
    size_t maxBatchSize; // Макс. batch
    std::chrono::seconds predictionWindow; // Окно предсказания
    double predictionThreshold; // Порог предсказания
    bool validate() const {
        return maxQueueSize > 0 && maxBatchSize > 0 && 
               predictionWindow.count() > 0 && predictionThreshold > 0.0;
    }
};
// PreloadTask — задача для предзагрузки (ключ, данные, время, приоритет)
struct PreloadTask {
    std::string key; // Ключ
    std::vector<uint8_t> data; // Данные
    std::chrono::steady_clock::time_point timestamp; // Время
    double priority; // Приоритет
};
// PreloadMetrics — метрики работы предзагрузки (очередь, активные, эффективность, точность)
struct PreloadMetrics {
    size_t queueSize; // Размер очереди
    size_t activeTasks; // Активные задачи
    double efficiency; // Эффективность
    double predictionAccuracy; // Точность предсказания
};

// PreloadManager — потокобезопасный менеджер предзагрузки данных в кэш, интеграция с DynamicCache, поддержка асинхронных задач и ML-предсказаний.
class PreloadManager {
public:
    explicit PreloadManager(const PreloadConfig& config); // Конструктор
    ~PreloadManager(); // Деструктор
    bool initialize(); // Инициализация
    bool preloadData(const std::string& key, const std::vector<uint8_t>& data); // Preload
    bool addData(const std::string& key, const std::vector<uint8_t>& data); // Добавить
    PreloadMetrics getMetrics() const; // Метрики
    void updateMetrics(); // Обновить метрики
    void setConfiguration(const PreloadConfig& config); // Установить конфиг
    PreloadConfig getConfiguration() const; // Получить конфиг
    void stop(); // Остановить
    void shutdown() { stop(); } // Завершить
    std::vector<std::string> getAllKeys() const; // Все ключи
    bool getDataForKey(const std::string& key, std::vector<uint8_t>& data) const; // Получить по ключу
    std::optional<std::vector<uint8_t>> getDataForKey(const std::string& key) const; // Получить (optional)
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl; // Реализация
    bool initialized;
    void startTaskProcessor(); // Запуск обработчика задач
    void processTask(const PreloadTask& task); // Обработка задачи
    bool predictNextAccess(const std::string& key); // ML-предсказание
    bool loadData(const std::string& key, std::vector<uint8_t>& data); // Загрузка данных
};

} // namespace experimental
} // namespace cache
} // namespace core
} // namespace cloud 

// Алиас для удобства использования
namespace cloud { namespace core {
using PreloadManager = cache::experimental::PreloadManager;
}} // namespace cloud::core 