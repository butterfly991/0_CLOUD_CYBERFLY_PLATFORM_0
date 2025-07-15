#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include "core/cache/base/AdaptiveCache.hpp"

void smokeTestAdaptiveCache() {
    std::cout << "Testing AdaptiveCache basic operations...\n";
    
    cloud::core::AdaptiveCache cache(1024 * 1024); // 1MB
    assert(cache.size() == 0);
    assert(cache.maxSize() == 1024 * 1024);
    
    std::cout << "[OK] AdaptiveCache smoke test\n";
}

void testAdaptiveCachePutGet() {
    std::cout << "Testing AdaptiveCache put/get operations...\n";
    
    cloud::core::AdaptiveCache cache(1024 * 1024); // 1MB
    
    // Добавляем данные
    std::vector<uint8_t> data1 = {1, 2, 3, 4, 5};
    std::vector<uint8_t> data2 = {6, 7, 8, 9, 10};
    
    cache.put("key1", data1);
    cache.put("key2", data2);
    
    assert(cache.size() == 2);
    
    // Получаем данные
    std::vector<uint8_t> retrievedData1, retrievedData2;
    assert(cache.get("key1", retrievedData1));
    assert(cache.get("key2", retrievedData2));
    
    assert(retrievedData1 == data1);
    assert(retrievedData2 == data2);
    
    // Проверяем несуществующий ключ
    std::vector<uint8_t> nonExistentData;
    assert(!cache.get("non_existent", nonExistentData));
    
    std::cout << "[OK] AdaptiveCache put/get test\n";
}

void testAdaptiveCacheAdaptation() {
    std::cout << "Testing AdaptiveCache adaptation...\n";
    
    cloud::core::AdaptiveCache cache(1024 * 1024); // 1MB
    
    // Добавляем данные
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    cache.put("adapt_key", data);
    
    assert(cache.size() == 1);
    
    // Адаптируем размер кэша
    size_t newSize = 2 * 1024 * 1024; // 2MB
    cache.adapt(newSize);
    
    assert(cache.maxSize() == newSize);
    assert(cache.size() == 1); // Данные должны сохраниться
    
    // Проверяем, что данные все еще доступны
    std::vector<uint8_t> retrievedData;
    assert(cache.get("adapt_key", retrievedData));
    assert(retrievedData == data);
    
    // Уменьшаем размер кэша
    size_t smallerSize = 512 * 1024; // 512KB
    cache.adapt(smallerSize);
    
    assert(cache.maxSize() == smallerSize);
    
    std::cout << "[OK] AdaptiveCache adaptation test\n";
}

void testAdaptiveCacheClear() {
    std::cout << "Testing AdaptiveCache clear operations...\n";
    
    cloud::core::AdaptiveCache cache(1024 * 1024); // 1MB
    
    // Добавляем данные
    for (int i = 0; i < 5; ++i) {
        std::string key = "clear_test_" + std::to_string(i);
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        cache.put(key, data);
    }
    
    assert(cache.size() == 5);
    
    // Очищаем кэш
    cache.clear();
    
    assert(cache.size() == 0);
    
    // Проверяем, что данные недоступны
    std::vector<uint8_t> retrievedData;
    assert(!cache.get("clear_test_0", retrievedData));
    
    std::cout << "[OK] AdaptiveCache clear test\n";
}

void testAdaptiveCacheSizeManagement() {
    std::cout << "Testing AdaptiveCache size management...\n";
    
    cloud::core::AdaptiveCache cache(1024 * 1024); // 1MB
    
    // Проверяем начальный размер
    assert(cache.size() == 0);
    assert(cache.maxSize() == 1024 * 1024);
    
    // Добавляем данные и проверяем размер
    for (int i = 0; i < 10; ++i) {
        std::string key = "size_test_" + std::to_string(i);
        std::vector<uint8_t> data(100, i); // 100 байт данных
        cache.put(key, data);
        assert(cache.size() == i + 1);
    }
    
    // Очищаем и проверяем размер
    cache.clear();
    assert(cache.size() == 0);
    
    std::cout << "[OK] AdaptiveCache size management test\n";
}

void testAdaptiveCacheStressTest() {
    std::cout << "Testing AdaptiveCache stress operations...\n";
    
    cloud::core::AdaptiveCache cache(1024 * 1024); // 1MB
    
    // Добавляем много данных
    const int numEntries = 1000;
    for (int i = 0; i < numEntries; ++i) {
        std::string key = "stress_test_" + std::to_string(i);
        std::vector<uint8_t> data(50, i % 256); // 50 байт данных
        cache.put(key, data);
    }
    
    // Проверяем, что все данные добавлены
    assert(cache.size() == numEntries);
    
    // Получаем все данные
    int retrievedCount = 0;
    for (int i = 0; i < numEntries; ++i) {
        std::string key = "stress_test_" + std::to_string(i);
        std::vector<uint8_t> data;
        if (cache.get(key, data)) {
            retrievedCount++;
        }
    }
    
    assert(retrievedCount == numEntries);
    
    // Очищаем кэш
    cache.clear();
    assert(cache.size() == 0);
    
    std::cout << "[OK] AdaptiveCache stress test\n";
}

void testAdaptiveCacheAdaptationStress() {
    std::cout << "Testing AdaptiveCache adaptation stress...\n";
    
    cloud::core::AdaptiveCache cache(1024 * 1024); // 1MB
    
    // Добавляем данные
    for (int i = 0; i < 100; ++i) {
        std::string key = "adapt_stress_" + std::to_string(i);
        std::vector<uint8_t> data(100, i % 256);
        cache.put(key, data);
    }
    
    assert(cache.size() == 100);
    
    // Многократно адаптируем размер
    for (int i = 0; i < 10; ++i) {
        size_t newSize = (i + 1) * 512 * 1024; // 512KB, 1MB, 1.5MB, ...
        cache.adapt(newSize);
        assert(cache.maxSize() == newSize);
    }
    
    // Проверяем, что данные сохранились
    assert(cache.size() == 100);
    
    // Проверяем доступность данных
    std::vector<uint8_t> data;
    assert(cache.get("adapt_stress_0", data));
    assert(data.size() == 100);
    
    std::cout << "[OK] AdaptiveCache adaptation stress test\n";
}

int main() {
    try {
        smokeTestAdaptiveCache();
        testAdaptiveCachePutGet();
        testAdaptiveCacheAdaptation();
        testAdaptiveCacheClear();
        testAdaptiveCacheSizeManagement();
        testAdaptiveCacheStressTest();
        testAdaptiveCacheAdaptationStress();
        std::cout << "All AdaptiveCache tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "AdaptiveCache test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "AdaptiveCache test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 