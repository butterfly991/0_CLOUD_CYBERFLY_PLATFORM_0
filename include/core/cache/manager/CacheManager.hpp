#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <chrono>
#include "core/cache/CacheConfig.hpp"
#include "core/cache/base/BaseCache.hpp"
#include "core/cache/metrics/CacheMetrics.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"

namespace cloud {
namespace core {
namespace cache {

// CacheManager — менеджер кэша, интеграция с DynamicCache, метрики, экспорт
class CacheManager {
public:
    explicit CacheManager(const CacheConfig& config); // Конструктор
    ~CacheManager(); // Деструктор
    bool initialize(); // Инициализация
    bool getData(const std::string& key, std::vector<uint8_t>& data); // Получить
    bool putData(const std::string& key, const std::vector<uint8_t>& data); // Сохранить
    void invalidateData(const std::string& key); // Инвалидировать
    void setConfiguration(const CacheConfig& config); // Установить конфиг
    CacheConfig getConfiguration() const; // Получить конфиг
    size_t getCacheSize() const; // Размер кэша
    size_t getEntryCount() const; // Кол-во записей
    CacheMetrics getMetrics() const; // Метрики
    void updateMetrics(); // Обновить метрики
    void cleanupCache(); // Очистить кэш
    std::unordered_map<std::string, std::vector<uint8_t>> exportAll() const; // Экспорт
    void shutdown(); // Завершение работы
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl; // Реализация
    bool initialized; // Статус
    mutable std::shared_mutex cacheMutex;
};

} // namespace cache
} // namespace core
} // namespace cloud
