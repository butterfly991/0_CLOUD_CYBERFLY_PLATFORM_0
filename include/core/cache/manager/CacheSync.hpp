#pragma once

#include "core/cache/CacheConfig.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>
#include "core/cache/manager/CacheManager.hpp"

namespace cloud {
namespace core {
namespace cache {

// CacheSync — синхронизация кэшей между ядрами, поддержка миграции и метрик
class CacheSync {
public:
    static CacheSync& getInstance(); // Singleton
    void registerCache(const std::string& kernelId, std::shared_ptr<CacheManager> cache); // Регистрация
    void unregisterCache(const std::string& kernelId); // Отмена регистрации
    void syncData(const std::string& sourceKernelId, const std::string& targetKernelId); // Синхронизация
    void syncAllCaches(); // Sync всех
    void migrateData(const std::string& sourceKernelId, const std::string& targetKernelId); // Миграция
    struct SyncStats {
        size_t syncCount; // Sync-операций
        size_t migrationCount; // Миграций
        std::chrono::steady_clock::time_point lastSync; // Последний sync
        double syncLatency; // Латентность
    };
    SyncStats getStats() const; // Получить метрики
private:
    CacheSync() = default;
    ~CacheSync() = default;
    CacheSync(const CacheSync&) = delete;
    CacheSync& operator=(const CacheSync&) = delete;
    std::unordered_map<std::string, std::shared_ptr<CacheManager>> caches_; // Кэш-карты
    mutable std::mutex mutex_;
    SyncStats stats_;
    bool validateSync(const std::string& sourceId, const std::string& targetId) const;
    void updateStats(size_t syncCount, size_t migrationCount, double latency);
};

} // namespace cache
} // namespace core
} // namespace cloud 