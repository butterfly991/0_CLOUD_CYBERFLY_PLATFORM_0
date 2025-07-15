#pragma once
#include <cstddef>
#include <chrono>
#include <nlohmann/json.hpp>

namespace cloud {
namespace core {
namespace cache {

// CacheMetrics — метрики кэша (размер, записи, hit rate, eviction)
struct CacheMetrics {
    size_t currentSize = 0; // Текущий размер (байт)
    size_t maxSize = 0;     // Макс. размер (байт)
    size_t entryCount = 0;  // Кол-во записей
    double hitRate = 0.0;   // Hit rate
    double evictionRate = 0.0; // Eviction rate
    size_t evictionCount = 0; // Кол-во вытеснений
    size_t requestCount = 0; // Кол-во запросов
    std::chrono::steady_clock::time_point lastUpdate; // Последнее обновление
    nlohmann::json toJson() const {
        return {
            {"currentSize", currentSize},
            {"maxSize", maxSize},
            {"entryCount", entryCount},
            {"hitRate", hitRate},
            {"evictionRate", evictionRate},
            {"evictionCount", evictionCount},
            {"requestCount", requestCount},
            {"lastUpdate", std::chrono::duration_cast<std::chrono::milliseconds>(lastUpdate.time_since_epoch()).count()}
        };
    }
};

} // namespace cache
} // namespace core
} // namespace cloud 