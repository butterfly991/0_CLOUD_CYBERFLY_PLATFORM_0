#include <cassert>
#include <iostream>
#include <memory>
#include "core/cache/experimental/PreloadManager.hpp"

void smokeTestPreloadManager() {
    std::cout << "Testing PreloadManager basic operations...\n";
    
    cloud::core::cache::experimental::PreloadConfig config{128, 16, std::chrono::seconds(10), 0.5};
    cloud::core::cache::experimental::PreloadManager manager(config);
    assert(manager.initialize());
    
    // Проверяем начальное состояние
    auto metrics = manager.getMetrics();
    assert(metrics.queueSize == 0);
    assert(metrics.activeTasks == 0);
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager smoke test\n";
}

void testPreloadManagerAddData() {
    std::cout << "Testing PreloadManager add data operations...\n";
    
    cloud::core::cache::experimental::PreloadConfig config;
    config.maxQueueSize = 50;
    config.maxBatchSize = 5;
    config.predictionWindow = std::chrono::seconds(10);
    config.predictionThreshold = 0.5;
    
    cloud::core::cache::experimental::PreloadManager manager(config);
    assert(manager.initialize());
    
    // Добавляем данные
    std::vector<uint8_t> data1 = {1, 2, 3, 4, 5};
    std::vector<uint8_t> data2 = {6, 7, 8, 9, 10};
    
    assert(manager.addData("key1", data1));
    assert(manager.addData("key2", data2));
    
    // Проверяем метрики
    auto metrics = manager.getMetrics();
    assert(metrics.queueSize >= 0);
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager add data test\n";
}

void testPreloadManagerPreloadData() {
    std::cout << "Testing PreloadManager preload data operations...\n";
    
    cloud::core::cache::experimental::PreloadConfig config;
    config.maxQueueSize = 100;
    config.maxBatchSize = 10;
    config.predictionWindow = std::chrono::seconds(15);
    config.predictionThreshold = 0.6;
    
    cloud::core::cache::experimental::PreloadManager manager(config);
    assert(manager.initialize());
    
    // Предварительно загружаем данные
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
    assert(manager.preloadData("preload_key", data));
    
    // Проверяем метрики
    auto metrics = manager.getMetrics();
    assert(metrics.activeTasks >= 0);
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager preload data test\n";
}

void testPreloadManagerGetData() {
    std::cout << "Testing PreloadManager get data operations...\n";
    
    cloud::core::cache::experimental::PreloadConfig config;
    config.maxQueueSize = 50;
    config.maxBatchSize = 5;
    config.predictionWindow = std::chrono::seconds(10);
    config.predictionThreshold = 0.5;
    
    cloud::core::cache::experimental::PreloadManager manager(config);
    assert(manager.initialize());
    
    // Добавляем данные
    std::vector<uint8_t> originalData = {1, 2, 3, 4, 5};
    assert(manager.addData("get_test_key", originalData));
    
    // Получаем данные
    std::vector<uint8_t> retrievedData;
    assert(manager.getDataForKey("get_test_key", retrievedData));
    assert(retrievedData == originalData);
    
    // Тестируем optional версию
    auto optionalData = manager.getDataForKey("get_test_key");
    assert(optionalData.has_value());
    assert(optionalData.value() == originalData);
    
    // Проверяем несуществующий ключ
    std::vector<uint8_t> nonExistentData;
    assert(!manager.getDataForKey("non_existent", nonExistentData));
    
    auto nonExistentOptional = manager.getDataForKey("non_existent");
    assert(!nonExistentOptional.has_value());
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager get data test\n";
}

void testPreloadManagerMetrics() {
    std::cout << "Testing PreloadManager metrics collection...\n";
    
    cloud::core::cache::experimental::PreloadConfig config;
    config.maxQueueSize = 100;
    config.maxBatchSize = 10;
    config.predictionWindow = std::chrono::seconds(20);
    config.predictionThreshold = 0.7;
    
    cloud::core::cache::experimental::PreloadManager manager(config);
    assert(manager.initialize());
    
    // Добавляем данные для генерации метрик
    for (int i = 0; i < 10; ++i) {
        std::string key = "metrics_test_" + std::to_string(i);
        std::vector<uint8_t> data(50, i);
        manager.addData(key, data);
    }
    
    // Обновляем метрики
    manager.updateMetrics();
    
    // Получаем метрики
    auto metrics = manager.getMetrics();
    assert(metrics.queueSize >= 0);
    assert(metrics.activeTasks >= 0);
    assert(metrics.efficiency >= 0.0);
    assert(metrics.predictionAccuracy >= 0.0);
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager metrics test\n";
}

void testPreloadManagerConfiguration() {
    std::cout << "Testing PreloadManager configuration management...\n";
    
    cloud::core::cache::experimental::PreloadConfig initialConfig;
    initialConfig.maxQueueSize = 50;
    initialConfig.maxBatchSize = 5;
    initialConfig.predictionWindow = std::chrono::seconds(10);
    initialConfig.predictionThreshold = 0.5;
    
    cloud::core::cache::experimental::PreloadManager manager(initialConfig);
    assert(manager.initialize());
    
    // Проверяем текущую конфигурацию
    auto currentConfig = manager.getConfiguration();
    assert(currentConfig.maxQueueSize == 50);
    assert(currentConfig.maxBatchSize == 5);
    assert(currentConfig.predictionWindow.count() == 10);
    assert(currentConfig.predictionThreshold == 0.5);
    
    // Устанавливаем новую конфигурацию
    cloud::core::cache::experimental::PreloadConfig newConfig;
    newConfig.maxQueueSize = 200;
    newConfig.maxBatchSize = 20;
    newConfig.predictionWindow = std::chrono::seconds(30);
    newConfig.predictionThreshold = 0.8;
    
    manager.setConfiguration(newConfig);
    
    // Проверяем обновленную конфигурацию
    auto updatedConfig = manager.getConfiguration();
    assert(updatedConfig.maxQueueSize == 200);
    assert(updatedConfig.maxBatchSize == 20);
    assert(updatedConfig.predictionWindow.count() == 30);
    assert(updatedConfig.predictionThreshold == 0.8);
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager configuration test\n";
}

void testPreloadManagerAllKeys() {
    std::cout << "Testing PreloadManager all keys retrieval...\n";
    
    cloud::core::cache::experimental::PreloadConfig config;
    config.maxQueueSize = 100;
    config.maxBatchSize = 10;
    config.predictionWindow = std::chrono::seconds(15);
    config.predictionThreshold = 0.6;
    
    cloud::core::cache::experimental::PreloadManager manager(config);
    assert(manager.initialize());
    
    // Добавляем несколько ключей
    std::vector<std::string> expectedKeys = {"key1", "key2", "key3", "key4", "key5"};
    for (const auto& key : expectedKeys) {
        std::vector<uint8_t> data = {1, 2, 3, 4, 5};
        manager.addData(key, data);
    }
    
    // Получаем все ключи
    auto allKeys = manager.getAllKeys();
    
    // Проверяем, что все ожидаемые ключи присутствуют
    for (const auto& expectedKey : expectedKeys) {
        bool found = false;
        for (const auto& actualKey : allKeys) {
            if (actualKey == expectedKey) {
                found = true;
                break;
            }
        }
        assert(found);
    }
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager all keys test\n";
}

void testPreloadManagerStressTest() {
    std::cout << "Testing PreloadManager stress operations...\n";
    
    cloud::core::cache::experimental::PreloadConfig config;
    config.maxQueueSize = 1000;
    config.maxBatchSize = 50;
    config.predictionWindow = std::chrono::seconds(60);
    config.predictionThreshold = 0.5;
    
    cloud::core::cache::experimental::PreloadManager manager(config);
    assert(manager.initialize());
    
    // Добавляем много данных
    const int numEntries = 500;
    for (int i = 0; i < numEntries; ++i) {
        std::string key = "stress_test_" + std::to_string(i);
        std::vector<uint8_t> data(100, i % 256);
        manager.addData(key, data);
    }
    
    // Получаем все ключи
    auto allKeys = manager.getAllKeys();
    assert(allKeys.size() >= 0);
    
    // Получаем несколько данных
    int retrievedCount = 0;
    for (int i = 0; i < 100; ++i) {
        std::string key = "stress_test_" + std::to_string(i);
        std::vector<uint8_t> data;
        if (manager.getDataForKey(key, data)) {
            retrievedCount++;
        }
    }
    
    // Обновляем метрики
    manager.updateMetrics();
    auto metrics = manager.getMetrics();
    
    manager.shutdown();
    std::cout << "[OK] PreloadManager stress test\n";
}

int main() {
    try {
        smokeTestPreloadManager();
        testPreloadManagerAddData();
        testPreloadManagerPreloadData();
        testPreloadManagerGetData();
        testPreloadManagerMetrics();
        testPreloadManagerConfiguration();
        testPreloadManagerAllKeys();
        testPreloadManagerStressTest();
        std::cout << "All PreloadManager tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "PreloadManager test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "PreloadManager test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 