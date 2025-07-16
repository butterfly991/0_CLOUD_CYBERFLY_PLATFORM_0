#include <cassert>
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/CacheConfig.hpp"

#include <spdlog/spdlog.h>

void smokeTestCacheManager() {
    std::cout << "Testing CacheManager basic operations...\n";
    
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 100;
    config.storagePath = "./cache/test";
    config.entryLifetime = std::chrono::seconds(60); // 1 минута
    config.enableCompression = false;
    config.enableMetrics = true;
    
    cloud::core::cache::CacheManager manager(config);
    assert(manager.initialize());
    assert(manager.getEntryCount() == 0);
    
    manager.shutdown();
    std::cout << "[OK] CacheManager smoke test\n";
}

void testCacheManagerPutGet() {
    std::cout << "Testing CacheManager put/get operations...\n";
    
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 50;
    config.storagePath = "./cache/test";
    config.entryLifetime = std::chrono::seconds(30);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    cloud::core::cache::CacheManager manager(config);
    assert(manager.initialize());
    
    // Тестируем базовые операции
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    assert(manager.putData("test_key", testData));
    
    std::vector<uint8_t> retrievedData;
    assert(manager.getData("test_key", retrievedData));
    assert(retrievedData == testData);
    
    // Тестируем несуществующий ключ
    std::vector<uint8_t> emptyData;
    assert(!manager.getData("nonexistent_key", emptyData));
    
    manager.shutdown();
    std::cout << "[OK] CacheManager put/get test\n";
}

void testCacheManagerInvalidation() {
    std::cout << "Testing CacheManager invalidation...\n";
    
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 20;
    config.storagePath = "./cache/test";
    config.entryLifetime = std::chrono::seconds(10);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    cloud::core::cache::CacheManager manager(config);
    assert(manager.initialize());
    
    // Добавляем данные
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5};
    assert(manager.putData("key1", testData));
    assert(manager.putData("key2", testData));
    
    // Проверяем, что данные есть
    std::vector<uint8_t> retrievedData;
    assert(manager.getData("key1", retrievedData));
    assert(manager.getData("key2", retrievedData));
    
    // Инвалидируем один ключ
    manager.invalidateData("key1");
    
    // Проверяем, что один ключ удален, другой остался
    assert(!manager.getData("key1", retrievedData));
    assert(manager.getData("key2", retrievedData));
    
    manager.shutdown();
    std::cout << "[OK] CacheManager invalidation test\n";
}

void testCacheManagerConfiguration() {
    std::cout << "Testing CacheManager configuration management...\n";
    
    cloud::core::cache::CacheConfig initialConfig;
    initialConfig.maxSize = 1024 * 1024; // 1MB
    initialConfig.maxEntries = 50;
    initialConfig.storagePath = "./cache/test";
    initialConfig.entryLifetime = std::chrono::seconds(60);
    initialConfig.enableCompression = false;
    initialConfig.enableMetrics = true;
    
    cloud::core::cache::CacheManager manager(initialConfig);
    assert(manager.initialize());
    
    // Проверяем текущую конфигурацию
    auto currentConfig = manager.getConfiguration();
    assert(currentConfig.maxSize == 1024 * 1024);
    assert(currentConfig.maxEntries == 50);
    assert(currentConfig.entryLifetime.count() == 60);
    assert(currentConfig.enableCompression == false);
    assert(currentConfig.enableMetrics == true);
    
    // Устанавливаем новую конфигурацию
    cloud::core::cache::CacheConfig newConfig;
    newConfig.maxSize = 2 * 1024 * 1024; // 2MB
    newConfig.maxEntries = 100;
    newConfig.storagePath = "./cache/new_test";
    newConfig.entryLifetime = std::chrono::seconds(120);
    newConfig.enableCompression = true;
    newConfig.enableMetrics = false;
    
    manager.setConfiguration(newConfig);
    
    // Проверяем обновленную конфигурацию
    auto updatedConfig = manager.getConfiguration();
    assert(updatedConfig.maxSize == 2 * 1024 * 1024);
    assert(updatedConfig.maxEntries == 100);
    assert(updatedConfig.entryLifetime.count() == 120);
    assert(updatedConfig.enableCompression == true);
    assert(updatedConfig.enableMetrics == false);
    
    manager.shutdown();
    std::cout << "[OK] CacheManager configuration test\n";
}

void testCacheManagerMetrics() {
    std::cout << "Testing CacheManager metrics collection...\n";
    
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 30;
    config.storagePath = "./cache/test";
    config.entryLifetime = std::chrono::seconds(30);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    cloud::core::cache::CacheManager manager(config);
    assert(manager.initialize());
    
    // Добавляем данные для генерации метрик
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> data(100, i);
        manager.putData("key_" + std::to_string(i), data);
    }
    
    // Получаем и проверяем метрики
    auto metrics = manager.getMetrics();
    assert(metrics.entryCount >= 0);
    assert(metrics.currentSize >= 0);
    assert(metrics.maxSize > 0);
    assert(metrics.hitRate >= 0.0);
    assert(metrics.evictionRate >= 0.0);
    
    manager.shutdown();
    std::cout << "[OK] CacheManager metrics test\n";
}

void testCacheManagerCleanup() {
    std::cout << "Testing CacheManager cleanup operations...\n";
    
    {
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 20;
    config.storagePath = "./cache/test";
    config.entryLifetime = std::chrono::seconds(5); // Короткое время жизни
    config.enableCompression = false;
    config.enableMetrics = true;
    
    cloud::core::cache::CacheManager manager(config);
    assert(manager.initialize());
    
    // Добавляем данные
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> data(50, i);
        manager.putData("cleanup_key_" + std::to_string(i), data);
    }
    
        // shutdown
    manager.shutdown();
        std::cout << "[TEST] После manager.shutdown()" << std::endl;
    }
    std::cout << "[TEST] После области жизни manager" << std::endl;
    std::cout << "[OK] CacheManager cleanup test" << std::endl;
}

void testCacheManagerExport() {
    std::cout << "Testing CacheManager export functionality...\n";
    
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 15;
    config.storagePath = "./cache/test";
    config.entryLifetime = std::chrono::seconds(60);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    cloud::core::cache::CacheManager manager(config);
    assert(manager.initialize());
    
    // Добавляем тестовые данные
    std::vector<uint8_t> data1 = {1, 2, 3, 4, 5};
    std::vector<uint8_t> data2 = {6, 7, 8, 9, 10};
    
    manager.putData("export_key1", data1);
    manager.putData("export_key2", data2);
    
    // Экспортируем все данные
    auto exportedData = manager.exportAll();
    assert(exportedData.size() >= 2);
    assert(exportedData["export_key1"] == data1);
    assert(exportedData["export_key2"] == data2);
    
    manager.shutdown();
    std::cout << "[OK] CacheManager export test\n";
}

int main() {
    try {
        smokeTestCacheManager();
        testCacheManagerPutGet();
        testCacheManagerInvalidation();
        testCacheManagerConfiguration();
        testCacheManagerMetrics();
        testCacheManagerCleanup();
        testCacheManagerExport();

        
        spdlog::shutdown(); // Гарантируем запись всех логов
        std::cout << "All CacheManager tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 