#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include "core/kernel/base/MicroKernel.hpp"
#include "core/kernel/advanced/SmartKernel.hpp"
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include "core/cache/CacheConfig.hpp"

void testKernelCacheManagerIntegration() {
    std::cout << "Testing Kernel-CacheManager integration...\n";
    
    // Создаем конфигурацию кэша
    cloud::core::cache::CacheConfig cacheConfig;
    cacheConfig.maxSize = 1024 * 1024 * 10; // 10MB
    cacheConfig.maxEntries = 1000;
    cacheConfig.enableMetrics = true;
    
    auto cacheManager = std::make_shared<cloud::core::cache::CacheManager>(cacheConfig);
    assert(cacheManager->initialize());
    
    // Создаем ядро
    auto kernel = std::make_shared<cloud::core::kernel::MicroKernel>("cache_kernel");
    assert(kernel->initialize());
    
    // Добавляем данные в кэш
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    assert(cacheManager->putData("kernel_test_key", testData));
    
    // Получаем данные из кэша
    std::vector<uint8_t> retrievedData;
    assert(cacheManager->getData("kernel_test_key", retrievedData));
    assert(retrievedData == testData);
    
    // Обновляем метрики кэша
    cacheManager->updateMetrics();
    
    // Проверяем метрики кэша
    auto cacheMetrics = cacheManager->getMetrics();
    assert(cacheMetrics.entryCount > 0);
    assert(cacheMetrics.currentSize > 0);
    
    // Проверяем метрики ядра
    auto kernelMetrics = kernel->getMetrics();
    assert(kernelMetrics.cpu_usage >= 0.0);
    assert(kernelMetrics.memory_usage >= 0.0);
    
    // Завершаем работу
    cacheManager->shutdown();
    kernel->shutdown();
    
    std::cout << "[OK] Kernel-CacheManager integration test\n";
}

void testKernelDynamicCacheIntegration() {
    std::cout << "Testing Kernel-DynamicCache integration...\n";
    
    // Создаем динамический кэш
    auto dynamicCache = std::make_shared<cloud::core::cache::DefaultDynamicCache>(100);
    
    // Создаем ядро
    auto kernel = std::make_shared<cloud::core::kernel::SmartKernel>();
    assert(kernel->initialize());
    
    // Добавляем данные в динамический кэш
    std::vector<uint8_t> data1 = {1, 2, 3, 4, 5};
    std::vector<uint8_t> data2 = {6, 7, 8, 9, 10};
    
    dynamicCache->put("dynamic_key_1", data1);
    dynamicCache->put("dynamic_key_2", data2);
    
    // Получаем данные
    auto retrieved1 = dynamicCache->get("dynamic_key_1");
    auto retrieved2 = dynamicCache->get("dynamic_key_2");
    
    assert(retrieved1.has_value());
    assert(retrieved2.has_value());
    assert(retrieved1.value() == data1);
    assert(retrieved2.value() == data2);
    
    // Проверяем размер кэша
    assert(dynamicCache->size() == 2);
    
    // Проверяем метрики ядра
    auto kernelMetrics = kernel->getMetrics();
    assert(kernelMetrics.cpu_usage >= 0.0);
    
    // Завершаем работу
    kernel->shutdown();
    
    std::cout << "[OK] Kernel-DynamicCache integration test\n";
}

void testKernelPreloadManagerIntegration() {
    std::cout << "Testing Kernel-PreloadManager integration...\n";
    
    // Создаем конфигурацию предварительной загрузки
    cloud::core::cache::experimental::PreloadConfig preloadConfig;
    preloadConfig.maxQueueSize = 50;
    preloadConfig.maxBatchSize = 10;
    preloadConfig.predictionWindow = std::chrono::seconds(30);
    preloadConfig.predictionThreshold = 0.7;
    
    auto preloadManager = std::make_shared<cloud::core::cache::experimental::PreloadManager>(preloadConfig);
    assert(preloadManager->initialize());
    
    // Создаем ядро
    auto kernel = std::make_shared<cloud::core::kernel::MicroKernel>("preload_kernel");
    assert(kernel->initialize());
    
    // Добавляем данные для предварительной загрузки
    for (int i = 0; i < 5; ++i) {
        std::string key = "preload_key_" + std::to_string(i);
        std::vector<uint8_t> data(100, i);
        preloadManager->addData(key, data);
    }
    
    // Предварительно загружаем данные
    std::vector<uint8_t> preloadData = {1, 2, 3, 4, 5, 6, 7, 8};
    assert(preloadManager->preloadData("preload_test_key", preloadData));
    
    // Получаем данные
    std::vector<uint8_t> retrievedData;
    assert(preloadManager->getDataForKey("preload_test_key", retrievedData));
    assert(retrievedData == preloadData);
    
    // Проверяем метрики предварительной загрузки
    auto preloadMetrics = preloadManager->getMetrics();
    assert(preloadMetrics.queueSize >= 0);
    assert(preloadMetrics.activeTasks >= 0);
    
    // Проверяем метрики ядра
    auto kernelMetrics = kernel->getMetrics();
    assert(kernelMetrics.cpu_usage >= 0.0);
    
    // Завершаем работу
    preloadManager->shutdown();
    kernel->shutdown();
    
    std::cout << "[OK] Kernel-PreloadManager integration test\n";
}

void testMultiKernelCacheIntegration() {
    std::cout << "Testing Multi-Kernel-Cache integration...\n";
    
    // Создаем кэш-менеджер
    cloud::core::cache::CacheConfig cacheConfig;
    cacheConfig.maxSize = 1024 * 1024 * 20; // 20MB
    cacheConfig.maxEntries = 2000;
    cacheConfig.enableMetrics = true;
    
    auto cacheManager = std::make_shared<cloud::core::cache::CacheManager>(cacheConfig);
    assert(cacheManager->initialize());
    
    // Создаем несколько ядер
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels;
    
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("multi_micro");
    auto smartKernel = std::make_shared<cloud::core::kernel::SmartKernel>();
    
    assert(microKernel->initialize());
    assert(smartKernel->initialize());
    
    kernels.push_back(microKernel);
    kernels.push_back(smartKernel);
    
    // Добавляем данные в кэш для каждого ядра
    for (size_t i = 0; i < kernels.size(); ++i) {
        std::string key = "multi_kernel_key_" + std::to_string(i);
        std::vector<uint8_t> data(200, static_cast<uint8_t>(i));
        assert(cacheManager->putData(key, data));
    }
    
    // Получаем данные из кэша для каждого ядра
    for (size_t i = 0; i < kernels.size(); ++i) {
        std::string key = "multi_kernel_key_" + std::to_string(i);
        std::vector<uint8_t> retrievedData;
        assert(cacheManager->getData(key, retrievedData));
        assert(retrievedData.size() == 200);
        assert(retrievedData[0] == static_cast<uint8_t>(i));
    }
    
    // Обновляем метрики кэша
    cacheManager->updateMetrics();
    
    // Проверяем метрики кэша
    auto cacheMetrics = cacheManager->getMetrics();
    assert(cacheMetrics.entryCount >= kernels.size());
    
    // Проверяем метрики всех ядер
    for (const auto& kernel : kernels) {
        auto metrics = kernel->getMetrics();
        assert(metrics.cpu_usage >= 0.0);
        assert(metrics.memory_usage >= 0.0);
    }
    
    // Завершаем работу
    cacheManager->shutdown();
    for (auto& kernel : kernels) {
        kernel->shutdown();
    }
    
    std::cout << "[OK] Multi-Kernel-Cache integration test\n";
}

void testCacheStressIntegration() {
    std::cout << "Testing Cache stress integration...\n";
    
    // Создаем кэш-менеджер
    cloud::core::cache::CacheConfig cacheConfig;
    cacheConfig.maxSize = 1024 * 1024 * 50; // 50MB
    cacheConfig.maxEntries = 5000;
    cacheConfig.enableMetrics = true;
    
    auto cacheManager = std::make_shared<cloud::core::cache::CacheManager>(cacheConfig);
    assert(cacheManager->initialize());
    
    // Создаем ядро
    auto kernel = std::make_shared<cloud::core::kernel::SmartKernel>();
    assert(kernel->initialize());
    
    // Добавляем много данных в кэш
    const int numEntries = 1000;
    for (int i = 0; i < numEntries; ++i) {
        std::string key = "stress_key_" + std::to_string(i);
        std::vector<uint8_t> data(100, i % 256);
        cacheManager->putData(key, data);
    }
    
    // Получаем данные из кэша
    int retrievedCount = 0;
    for (int i = 0; i < numEntries; ++i) {
        std::string key = "stress_key_" + std::to_string(i);
        std::vector<uint8_t> retrievedData;
        if (cacheManager->getData(key, retrievedData)) {
            retrievedCount++;
        }
    }
    
    assert(retrievedCount > 0);
    
    // Обновляем метрики кэша
    cacheManager->updateMetrics();
    
    // Проверяем метрики
    auto cacheMetrics = cacheManager->getMetrics();
    assert(cacheMetrics.entryCount > 0);
    assert(cacheMetrics.currentSize > 0);
    
    auto kernelMetrics = kernel->getMetrics();
    assert(kernelMetrics.cpu_usage >= 0.0);
    
    // Завершаем работу
    cacheManager->shutdown();
    kernel->shutdown();
    
    std::cout << "[OK] Cache stress integration test\n";
}

int main() {
    try {
        testKernelCacheManagerIntegration();
        testKernelDynamicCacheIntegration();
        testKernelPreloadManagerIntegration();
        testMultiKernelCacheIntegration();
        testCacheStressIntegration();
        std::cout << "All Cache-Kernel integration tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Cache-Kernel integration test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Cache-Kernel integration test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 