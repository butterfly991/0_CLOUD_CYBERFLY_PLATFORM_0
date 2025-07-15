#include "core/cache/manager/CacheSync.hpp"
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace cache {

CacheSync& CacheSync::getInstance() {
    static CacheSync instance;
    return instance;
}

void CacheSync::registerCache(const std::string& kernelId, std::shared_ptr<CacheManager> cache) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cache) {
        spdlog::error("CacheSync: попытка зарегистрировать nullptr cache для kernelId='{}'", kernelId);
        return;
    }
    if (caches_.find(kernelId) != caches_.end()) {
        spdlog::warn("Cache for kernel '{}' already registered", kernelId);
        return;
    }
    caches_[kernelId] = cache;
    spdlog::info("CacheSync: зарегистрирован кэш для kernelId='{}'", kernelId);
}

void CacheSync::unregisterCache(const std::string& kernelId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = caches_.find(kernelId);
    if (it == caches_.end()) {
        spdlog::warn("Cache for kernel '{}' not found", kernelId);
        return;
    }
    caches_.erase(it);
    spdlog::info("Cache for kernel '{}' unregistered", kernelId);
}

void CacheSync::syncData(const std::string& sourceKernelId, const std::string& targetKernelId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!validateSync(sourceKernelId, targetKernelId)) return;
    auto startTime = std::chrono::steady_clock::now();
    auto sourceCache = caches_[sourceKernelId];
    auto targetCache = caches_[targetKernelId];
    // Экспортируем все данные из source и импортируем в target
    auto data = sourceCache->exportAll();
    for (const auto& [key, value] : data) {
        targetCache->putData(key, value);
    }
    auto endTime = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    updateStats(1, 0, latency);
    spdlog::info("Data synced from kernel '{}' to '{}' in {}ms", sourceKernelId, targetKernelId, latency);
}

void CacheSync::syncAllCaches() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto startTime = std::chrono::steady_clock::now();
    size_t syncCount = 0;
    for (const auto& [sourceId, sourceCache] : caches_) {
        for (const auto& [targetId, targetCache] : caches_) {
            if (sourceId != targetId) {
                auto data = sourceCache->exportAll();
                for (const auto& [key, value] : data) {
                    targetCache->putData(key, value);
                }
                syncCount++;
            }
        }
    }
    auto endTime = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    updateStats(syncCount, 0, latency);
    spdlog::info("All caches synced in {}ms", latency);
}

void CacheSync::migrateData(const std::string& sourceKernelId, const std::string& targetKernelId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!validateSync(sourceKernelId, targetKernelId)) return;
    auto startTime = std::chrono::steady_clock::now();
    auto sourceCache = caches_[sourceKernelId];
    auto targetCache = caches_[targetKernelId];
    // Экспортируем все данные из source и импортируем в target, затем очищаем source
    auto data = sourceCache->exportAll();
    for (const auto& [key, value] : data) {
        targetCache->putData(key, value);
    }
    for (const auto& [key, _] : data) {
        sourceCache->invalidateData(key);
    }
    auto endTime = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    updateStats(0, 1, latency);
    spdlog::info("Data migrated from kernel '{}' to '{}' in {}ms", sourceKernelId, targetKernelId, latency);
}

CacheSync::SyncStats CacheSync::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

bool CacheSync::validateSync(const std::string& sourceId, const std::string& targetId) const {
    if (sourceId == targetId) {
        spdlog::warn("Source and target kernels are the same: '{}'", sourceId);
        return false;
    }
    if (caches_.find(sourceId) == caches_.end()) {
        spdlog::error("Source kernel '{}' not found", sourceId);
        return false;
    }
    if (caches_.find(targetId) == caches_.end()) {
        spdlog::error("Target kernel '{}' not found", targetId);
        return false;
    }
    return true;
}

void CacheSync::updateStats(size_t syncCount, size_t migrationCount, double latency) {
    stats_.syncCount += syncCount;
    stats_.migrationCount += migrationCount;
    stats_.lastSync = std::chrono::steady_clock::now();
    stats_.syncLatency = (stats_.syncLatency + latency) / 2.0;
}

} // namespace cache
} // namespace core
} // namespace cloud