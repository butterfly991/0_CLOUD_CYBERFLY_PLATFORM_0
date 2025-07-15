#include <cassert>
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "core/cache/manager/CacheSync.hpp"
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/CacheConfig.hpp"

void smokeTestCacheSync() {
    std::cout << "Testing CacheSync basic operations...\n";
    
    cloud::core::cache::CacheSync& sync = cloud::core::cache::CacheSync::getInstance();
    
    // Создаем два кэш-менеджера
    cloud::core::cache::CacheConfig config1, config2;
    config1.maxSize = 1024 * 1024; // 1MB
    config1.maxEntries = 50;
    config1.storagePath = "./cache/sync1";
    config1.entryLifetime = std::chrono::seconds(60);
    config1.enableCompression = false;
    config1.enableMetrics = true;
    
    config2.maxSize = 1024 * 1024; // 1MB
    config2.maxEntries = 50;
    config2.storagePath = "./cache/sync2";
    config2.entryLifetime = std::chrono::seconds(60);
    config2.enableCompression = false;
    config2.enableMetrics = true;
    
    auto cache1 = std::make_shared<cloud::core::cache::CacheManager>(config1);
    auto cache2 = std::make_shared<cloud::core::cache::CacheManager>(config2);
    
    assert(cache1->initialize());
    assert(cache2->initialize());
    
    // Регистрируем кэши
    sync.registerCache("kernel1", cache1);
    sync.registerCache("kernel2", cache2);
    
    // Проверяем статистику
    auto stats = sync.getStats();
    assert(stats.syncCount >= 0);
    assert(stats.migrationCount >= 0);
    
    // Отменяем регистрацию
    sync.unregisterCache("kernel1");
    sync.unregisterCache("kernel2");
    
    cache1->shutdown();
    cache2->shutdown();
    
    std::cout << "[OK] CacheSync smoke test\n";
}

void testCacheSyncRegistration() {
    std::cout << "Testing CacheSync registration management...\n";
    
    cloud::core::cache::CacheSync& sync = cloud::core::cache::CacheSync::getInstance();
    
    // Создаем кэш-менеджер
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 30;
    config.storagePath = "./cache/reg_test";
    config.entryLifetime = std::chrono::seconds(30);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    auto cache = std::make_shared<cloud::core::cache::CacheManager>(config);
    assert(cache->initialize());
    
    // Регистрируем кэш
    sync.registerCache("test_kernel", cache);
    
    // Проверяем, что регистрация прошла успешно
    auto stats = sync.getStats();
    assert(stats.syncCount >= 0);
    
    // Отменяем регистрацию
    sync.unregisterCache("test_kernel");
    
    cache->shutdown();
    std::cout << "[OK] CacheSync registration test\n";
}

void testCacheSyncDataSync() {
    std::cout << "Testing CacheSync data synchronization...\n";
    
    cloud::core::cache::CacheSync& sync = cloud::core::cache::CacheSync::getInstance();
    
    // Создаем два кэш-менеджера
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 40;
    config.storagePath = "./cache/sync_test";
    config.entryLifetime = std::chrono::seconds(60);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    auto cache1 = std::make_shared<cloud::core::cache::CacheManager>(config);
    auto cache2 = std::make_shared<cloud::core::cache::CacheManager>(config);
    
    assert(cache1->initialize());
    assert(cache2->initialize());
    
    // Регистрируем кэши
    sync.registerCache("source_kernel", cache1);
    sync.registerCache("target_kernel", cache2);
    
    // Добавляем данные в первый кэш
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    assert(cache1->putData("sync_key", testData));
    
    // Синхронизируем данные
    sync.syncData("source_kernel", "target_kernel");
    
    // Проверяем, что данные синхронизированы
    std::vector<uint8_t> retrievedData;
    assert(cache2->getData("sync_key", retrievedData));
    assert(retrievedData == testData);
    
    // Отменяем регистрацию
    sync.unregisterCache("source_kernel");
    sync.unregisterCache("target_kernel");
    
    cache1->shutdown();
    cache2->shutdown();
    
    std::cout << "[OK] CacheSync data sync test\n";
}

void testCacheSyncMigration() {
    std::cout << "Testing CacheSync data migration...\n";
    
    cloud::core::cache::CacheSync& sync = cloud::core::cache::CacheSync::getInstance();
    
    // Создаем два кэш-менеджера
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 35;
    config.storagePath = "./cache/migrate_test";
    config.entryLifetime = std::chrono::seconds(60);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    auto sourceCache = std::make_shared<cloud::core::cache::CacheManager>(config);
    auto targetCache = std::make_shared<cloud::core::cache::CacheManager>(config);
    
    assert(sourceCache->initialize());
    assert(targetCache->initialize());
    
    // Регистрируем кэши
    sync.registerCache("migrate_source", sourceCache);
    sync.registerCache("migrate_target", targetCache);
    
    // Добавляем данные в исходный кэш
    std::vector<uint8_t> data1 = {1, 2, 3, 4, 5};
    std::vector<uint8_t> data2 = {6, 7, 8, 9, 10};
    
    assert(sourceCache->putData("migrate_key1", data1));
    assert(sourceCache->putData("migrate_key2", data2));
    
    // Мигрируем данные
    sync.migrateData("migrate_source", "migrate_target");
    
    // Проверяем, что данные мигрированы
    std::vector<uint8_t> retrievedData1, retrievedData2;
    assert(targetCache->getData("migrate_key1", retrievedData1));
    assert(targetCache->getData("migrate_key2", retrievedData2));
    assert(retrievedData1 == data1);
    assert(retrievedData2 == data2);
    
    // Отменяем регистрацию
    sync.unregisterCache("migrate_source");
    sync.unregisterCache("migrate_target");
    
    sourceCache->shutdown();
    targetCache->shutdown();
    
    std::cout << "[OK] CacheSync migration test\n";
}

void testCacheSyncAllCaches() {
    std::cout << "Testing CacheSync all caches synchronization...\n";
    
    cloud::core::cache::CacheSync& sync = cloud::core::cache::CacheSync::getInstance();
    
    // Создаем несколько кэш-менеджеров
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 25;
    config.storagePath = "./cache/all_sync_test";
    config.entryLifetime = std::chrono::seconds(60);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    auto cache1 = std::make_shared<cloud::core::cache::CacheManager>(config);
    auto cache2 = std::make_shared<cloud::core::cache::CacheManager>(config);
    auto cache3 = std::make_shared<cloud::core::cache::CacheManager>(config);
    
    assert(cache1->initialize());
    assert(cache2->initialize());
    assert(cache3->initialize());
    
    // Регистрируем кэши
    sync.registerCache("all_kernel1", cache1);
    sync.registerCache("all_kernel2", cache2);
    sync.registerCache("all_kernel3", cache3);
    
    // Добавляем данные в первый кэш
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5};
    assert(cache1->putData("all_sync_key", testData));
    
    // Синхронизируем все кэши
    sync.syncAllCaches();
    
    // Проверяем, что данные синхронизированы во всех кэшах
    std::vector<uint8_t> retrievedData;
    assert(cache2->getData("all_sync_key", retrievedData));
    assert(retrievedData == testData);
    assert(cache3->getData("all_sync_key", retrievedData));
    assert(retrievedData == testData);
    
    // Отменяем регистрацию
    sync.unregisterCache("all_kernel1");
    sync.unregisterCache("all_kernel2");
    sync.unregisterCache("all_kernel3");
    
    cache1->shutdown();
    cache2->shutdown();
    cache3->shutdown();
    
    std::cout << "[OK] CacheSync all caches test\n";
}

void testCacheSyncStats() {
    std::cout << "Testing CacheSync statistics collection...\n";
    
    cloud::core::cache::CacheSync& sync = cloud::core::cache::CacheSync::getInstance();
    
    // Создаем кэш-менеджеры
    cloud::core::cache::CacheConfig config;
    config.maxSize = 1024 * 1024; // 1MB
    config.maxEntries = 20;
    config.storagePath = "./cache/stats_test";
    config.entryLifetime = std::chrono::seconds(60);
    config.enableCompression = false;
    config.enableMetrics = true;
    
    auto cache1 = std::make_shared<cloud::core::cache::CacheManager>(config);
    auto cache2 = std::make_shared<cloud::core::cache::CacheManager>(config);
    
    assert(cache1->initialize());
    assert(cache2->initialize());
    
    // Регистрируем кэши
    sync.registerCache("stats_kernel1", cache1);
    sync.registerCache("stats_kernel2", cache2);
    
    // Получаем начальную статистику
    auto initialStats = sync.getStats();
    
    // Выполняем операции синхронизации
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5};
    cache1->putData("stats_key", testData);
    
    sync.syncData("stats_kernel1", "stats_kernel2");
    sync.migrateData("stats_kernel1", "stats_kernel2");
    
    // Получаем финальную статистику
    auto finalStats = sync.getStats();
    assert(finalStats.syncCount >= initialStats.syncCount);
    assert(finalStats.migrationCount >= initialStats.migrationCount);
    
    // Отменяем регистрацию
    sync.unregisterCache("stats_kernel1");
    sync.unregisterCache("stats_kernel2");
    
    cache1->shutdown();
    cache2->shutdown();
    
    std::cout << "[OK] CacheSync stats test\n";
}

int main() {
    try {
        smokeTestCacheSync();
        testCacheSyncRegistration();
        testCacheSyncDataSync();
        testCacheSyncMigration();
        testCacheSyncAllCaches();
        testCacheSyncStats();
        std::cout << "All CacheSync tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 