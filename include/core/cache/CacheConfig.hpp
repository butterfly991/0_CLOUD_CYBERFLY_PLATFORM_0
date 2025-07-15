#pragma once
#include <string>
#include <chrono>

namespace cloud {
namespace core {
namespace cache {

// CacheConfig — параметры кэша (размер, lifetime, storage, метрики)
struct CacheConfig {
    size_t maxSize = 1024 * 1024 * 100; // Макс. размер (100 MB)
    size_t maxEntries = 10000;           // Макс. записи
    std::chrono::seconds entryLifetime = std::chrono::seconds(3600); // Lifetime (1 час)
    bool enableCompression = false;      // Сжатие
    bool enableMetrics = true;           // Метрики
    std::string storagePath = "./cache"; // Путь
    bool validate() const {
        return maxSize > 0 && maxEntries > 0 && entryLifetime.count() > 0;
    }
};

} // namespace cache
} // namespace core
} // namespace cloud 