#pragma once
#include <string>
#include <vector>
#include <memory>
#include "core/drivers/ARMDriver.hpp"
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/base/BaseCache.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/CacheConfig.hpp"

namespace cloud {
namespace core {
namespace security {

// CryptoKernel — ядро для криптографических операций
class CryptoKernel {
public:
    explicit CryptoKernel(const std::string& id);
    ~CryptoKernel();
    bool initialize(); // Инициализация
    void shutdown();   // Завершение работы
    bool execute(const std::vector<uint8_t>& data, std::vector<uint8_t>& result); // Криптозадача
    void updateMetrics(); // Обновить метрики
    std::string getId() const; // Получить ID
private:
    std::string id; // ID ядра
    std::unique_ptr<drivers::ARMDriver> armDriver; // ARM/NEON
    std::unique_ptr<cache::CacheManager> cache; // Кэш
    std::unique_ptr<cache::DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache; // Динамический кэш
};

} // namespace security
} // namespace core
} // namespace cloud
