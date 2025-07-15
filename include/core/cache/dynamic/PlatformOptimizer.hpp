#pragma once

#include <memory>
#include <string>
#include "core/cache/CacheConfig.hpp"

namespace cloud {
namespace core {
namespace cache {

// PlatformOptimizer — оптимизация кэша под платформу (ARM/Apple, Linux, AVX, Metal)
class PlatformOptimizer {
public:
    PlatformOptimizer() = default;
    ~PlatformOptimizer() = default;
    PlatformOptimizer(PlatformOptimizer&&) = default;
    PlatformOptimizer& operator=(PlatformOptimizer&&) = default;
    static PlatformOptimizer& getInstance(); // Singleton
    void optimizeCache(CacheConfig& config) const; // Оптимизация кэша
    CacheConfig getOptimalConfig() const; // Оптимальный конфиг
    bool isPlatformSupported() const; // Поддержка платформы
    std::string getPlatformInfo() const; // Инфо о платформе
private:
    PlatformOptimizer(const PlatformOptimizer&) = delete;
    PlatformOptimizer& operator=(const PlatformOptimizer&) = delete;
#ifdef CLOUD_PLATFORM_APPLE_ARM
    void optimizeForAppleARM(CacheConfig& config); // ARM/Apple
    void configureMetalAcceleration(CacheConfig& config); // Metal
#elif defined(CLOUD_PLATFORM_LINUX_X64)
    void optimizeForLinuxX64(CacheConfig& config); // Linux
    void configureAVXAcceleration(CacheConfig& config); // AVX
#endif
    void detectHardwareCapabilities() const;
    void adjustConfigForHardware(CacheConfig& config) const;
};

} // namespace cache
} // namespace core
} // namespace cloud 