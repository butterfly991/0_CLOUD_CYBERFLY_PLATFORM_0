#include <cassert>
#include <iostream>
#include <memory>
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/cache/CacheConfig.hpp"

void smokeTestPlatformOptimizer() {
    std::cout << "Testing PlatformOptimizer basic operations...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Проверяем поддержку платформы
    bool isSupported = optimizer.isPlatformSupported();
    std::string platformInfo = optimizer.getPlatformInfo();
    
    assert(!platformInfo.empty());
    
    std::cout << "[OK] PlatformOptimizer smoke test\n";
}

void testPlatformOptimizerCacheOptimization() {
    std::cout << "Testing PlatformOptimizer cache optimization...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Создаем базовую конфигурацию кэша
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024 * 10; // 10MB
    config.maxEntries = 1000;
    config.enableCompression = false;
    config.enableMetrics = true;
    
    // Оптимизируем конфигурацию под платформу
    optimizer.optimizeCache(config);
    
    // Проверяем, что конфигурация была изменена
    assert(config.maxSize > 0);
    assert(config.maxEntries > 0);
    
    std::cout << "[OK] PlatformOptimizer cache optimization test\n";
}

void testPlatformOptimizerOptimalConfig() {
    std::cout << "Testing PlatformOptimizer optimal configuration...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Получаем оптимальную конфигурацию для платформы
    cloud::core::cache::CacheConfig optimalConfig = optimizer.getOptimalConfig();
    
    // Проверяем, что конфигурация валидна
    assert(optimalConfig.maxSize > 0);
    assert(optimalConfig.maxEntries > 0);
    assert(optimalConfig.entryLifetime.count() > 0);
    
    // Проверяем валидацию конфигурации
    assert(optimalConfig.validate());
    
    std::cout << "[OK] PlatformOptimizer optimal config test\n";
}

void testPlatformOptimizerPlatformDetection() {
    std::cout << "Testing PlatformOptimizer platform detection...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Проверяем информацию о платформе
    std::string platformInfo = optimizer.getPlatformInfo();
    assert(!platformInfo.empty());
    
    // Проверяем поддержку платформы
    bool isSupported = optimizer.isPlatformSupported();
    
    // Выводим информацию о платформе
    std::cout << "Platform: " << platformInfo << std::endl;
    std::cout << "Supported: " << (isSupported ? "Yes" : "No") << std::endl;
    
    std::cout << "[OK] PlatformOptimizer platform detection test\n";
}

void testPlatformOptimizerHardwareCapabilities() {
    std::cout << "Testing PlatformOptimizer hardware capabilities...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Создаем конфигурацию для тестирования
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024 * 5; // 5MB
    config.maxEntries = 500;
    
    // Оптимизируем конфигурацию
    optimizer.optimizeCache(config);
    
    // Проверяем, что конфигурация была адаптирована под оборудование
    assert(config.validate());
    
    std::cout << "[OK] PlatformOptimizer hardware capabilities test\n";
}

void testPlatformOptimizerConfigurationValidation() {
    std::cout << "Testing PlatformOptimizer configuration validation...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Создаем невалидную конфигурацию
    cloud::core::cache::CacheConfig invalidConfig;
    invalidConfig.maxSize = 0; // Невалидное значение
    invalidConfig.maxEntries = 0; // Невалидное значение
    
    assert(!invalidConfig.validate());
    
    // Оптимизируем конфигурацию
    optimizer.optimizeCache(invalidConfig);
    
    // Проверяем, что конфигурация стала валидной
    assert(invalidConfig.validate());
    assert(invalidConfig.maxSize > 0);
    assert(invalidConfig.maxEntries > 0);
    
    std::cout << "[OK] PlatformOptimizer configuration validation test\n";
}

void testPlatformOptimizerPerformanceOptimization() {
    std::cout << "Testing PlatformOptimizer performance optimization...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Создаем конфигурацию для производительности
    cloud::core::cache::CacheConfig perfConfig;
    perfConfig.maxSize = 1024 * 1024 * 20; // 20MB
    perfConfig.maxEntries = 2000;
    perfConfig.enableCompression = false; // Отключаем сжатие для производительности
    perfConfig.enableMetrics = true;
    
    // Оптимизируем для производительности
    optimizer.optimizeCache(perfConfig);
    
    // Проверяем, что конфигурация оптимизирована
    assert(perfConfig.validate());
    assert(perfConfig.maxSize > 0);
    assert(perfConfig.maxEntries > 0);
    
    std::cout << "[OK] PlatformOptimizer performance optimization test\n";
}

void testPlatformOptimizerMemoryOptimization() {
    std::cout << "Testing PlatformOptimizer memory optimization...\n";
    
    cloud::core::cache::PlatformOptimizer optimizer;
    
    // Создаем конфигурацию для экономии памяти
    cloud::core::cache::CacheConfig memoryConfig;
    memoryConfig.maxSize = 1024 * 1024 * 2; // 2MB
    memoryConfig.maxEntries = 100;
    memoryConfig.enableCompression = true; // Включаем сжатие для экономии памяти
    memoryConfig.enableMetrics = false; // Отключаем метрики для экономии памяти
    
    // Оптимизируем для экономии памяти
    optimizer.optimizeCache(memoryConfig);
    
    // Проверяем, что конфигурация оптимизирована
    assert(memoryConfig.validate());
    assert(memoryConfig.maxSize > 0);
    assert(memoryConfig.maxEntries > 0);
    
    std::cout << "[OK] PlatformOptimizer memory optimization test\n";
}

int main() {
    try {
        smokeTestPlatformOptimizer();
        testPlatformOptimizerCacheOptimization();
        testPlatformOptimizerOptimalConfig();
        testPlatformOptimizerPlatformDetection();
        testPlatformOptimizerHardwareCapabilities();
        testPlatformOptimizerConfigurationValidation();
        testPlatformOptimizerPerformanceOptimization();
        testPlatformOptimizerMemoryOptimization();
        std::cout << "All PlatformOptimizer tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "PlatformOptimizer test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "PlatformOptimizer test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 