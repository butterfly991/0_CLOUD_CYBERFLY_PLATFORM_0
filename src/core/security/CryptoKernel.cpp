#include "core/security/CryptoKernel.hpp"
#include <spdlog/spdlog.h>
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/CacheConfig.hpp"

namespace cloud {
namespace core {
namespace security {

CryptoKernel::CryptoKernel(const std::string& id_) : id(id_) {
    armDriver = std::make_unique<drivers::ARMDriver>();
    
    // Создаем правильную конфигурацию для CacheManager
    cache::CacheConfig cacheConfig;
    cacheConfig.maxSize = 1024 * 1024 * 10; // 10MB
    cacheConfig.maxEntries = 1000;
    cacheConfig.entryLifetime = std::chrono::seconds(3600); // 1 час
    cacheConfig.enableCompression = false;
    cacheConfig.enableMetrics = true;
    cacheConfig.storagePath = "./cache/crypto";
    
    cache = std::make_unique<cache::CacheManager>(cacheConfig);
    dynamicCache = std::make_unique<cache::DynamicCache<std::string, std::vector<uint8_t>>>(64);
}

CryptoKernel::~CryptoKernel() {
    shutdown();
}

bool CryptoKernel::initialize() {
    spdlog::info("CryptoKernel: initialize start");
    std::string storagePath = "./recovery_points/crypto";
    size_t cacheSize = 1024 * 1024; // 1MB по умолчанию
    // Проверка и дефолты
    if (storagePath.empty()) {
        storagePath = "./recovery_points/crypto";
        spdlog::warn("CryptoKernel: storagePath был пуст, установлен по умолчанию: {}", storagePath);
    }
    if (cacheSize == 0) {
        cacheSize = 1024 * 1024;
        spdlog::warn("CryptoKernel: cacheSize был 0, установлен по умолчанию: {}", cacheSize);
    }
    spdlog::info("CryptoKernel: параметры: storagePath='{}', cacheSize={}", storagePath, cacheSize);
    bool accel = armDriver->initialize();
    bool cacheInit = cache->initialize();
    return accel && cacheInit;
}

void CryptoKernel::shutdown() {
    spdlog::info("CryptoKernel[{}]: завершение работы", id);
    if (armDriver) {
        armDriver->shutdown();
    }
    if (cache) {
        cache->shutdown();
    }
    if (dynamicCache) {
        dynamicCache->clear();
    }
}

bool CryptoKernel::execute(const std::vector<uint8_t>& data, std::vector<uint8_t>& result) {
    spdlog::debug("CryptoKernel[{}]: выполнение криптографической задачи", id);
    result = data;
    
    // Применяем простое криптографическое преобразование
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = result[i] ^ 0xAA; // XOR с константой
        result[i] = (result[i] * 7 + 13) % 256; // Простое преобразование
    }
    
    if (cache) {
        cache->putData("crypto", result);
    }
    if (dynamicCache) {
        dynamicCache->put("crypto", result);
    }
    return true;
}

void CryptoKernel::updateMetrics() {
    if (cache) {
        cache->updateMetrics();
    }
}

std::string CryptoKernel::getId() const {
    return id;
}

} // namespace security
} // namespace core
} // namespace cloud
