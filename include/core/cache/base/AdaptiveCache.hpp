#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cloud {
namespace core {

// AdaptiveCache — адаптивный кэш с динамическим изменением размера
class AdaptiveCache {
public:
    AdaptiveCache(size_t maxSize); // Конструктор
    ~AdaptiveCache(); // Деструктор
    bool get(const std::string& key, std::vector<uint8_t>& data); // Получить
    void put(const std::string& key, const std::vector<uint8_t>& data); // Сохранить
    void adapt(size_t newMaxSize); // Адаптировать размер
    void clear(); // Очистить
    size_t size() const; // Размер
    size_t maxSize() const; // Макс. размер
private:
    size_t maxSize_; // Макс. размер
    std::unordered_map<std::string, std::vector<uint8_t>> cache_; // Кэш
    mutable std::mutex mutex_;
};

} // namespace core
} // namespace cloud
