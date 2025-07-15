#include <cassert>
#include <iostream>
#include "core/cache/dynamic/DynamicCache.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <thread>

void smokeTestDynamicCache() {
    std::cout << "Testing DynamicCache basic operations...\n";
    
    // Используем правильный тип с конкретными параметрами
    cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>> cache(4);
    
    // Добавляем элементы
    cache.put("a", {1});
    std::cout << "After adding 'a': size = " << cache.size() << std::endl;
    
    cache.put("b", {2});
    std::cout << "After adding 'b': size = " << cache.size() << std::endl;
    
    cache.put("c", {3});
    std::cout << "After adding 'c': size = " << cache.size() << std::endl;
    
    cache.put("d", {4});
    std::cout << "After adding 'd': size = " << cache.size() << std::endl;
    
    // Проверяем размер
    std::cout << "Final size before assertion: " << cache.size() << std::endl;
    assert(cache.size() == 4);
    
    // Тестируем LRU eviction
    cache.put("e", {5}); // Должен вытеснить "a"
    std::cout << "After adding 'e': size = " << cache.size() << std::endl;
    assert(cache.size() == 4);
    
    // Проверяем, что "e" добавлен, а "a" вытеснен
    auto v = cache.get("e");
    assert(v && (*v)[0] == 5);
    
    auto a = cache.get("a");
    assert(!a); // "a" должен быть вытеснен
    
    // Тестируем удаление
    cache.remove("e");
    assert(!cache.get("e"));
    
    // Тестируем очистку
    cache.clear();
    assert(cache.size() == 0);
    
    std::cout << "[OK] DynamicCache smoke test\n";
}

void stressTestDynamicCache() {
    std::cout << "Testing DynamicCache stress operations...\n";
    
    cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>> cache(128);
    
    // Добавляем много элементов
    for (int i = 0; i < 1000; ++i) {
        cache.put(std::to_string(i), {static_cast<uint8_t>(i % 256)});
    }
    
    // Проверяем, что размер не превышает лимит
    assert(cache.size() <= 128);
    
    // Удаляем все элементы
    for (int i = 0; i < 1000; ++i) {
        cache.remove(std::to_string(i));
    }
    
    assert(cache.size() == 0);
    std::cout << "[OK] DynamicCache stress test\n";
}

void testDynamicCacheTTL() {
    std::cout << "Testing DynamicCache TTL functionality...\n";
    
    cloud::core::cache::DynamicCache<std::string, std::vector<uint8_t>> cache(10);
    
    // Добавляем элемент с TTL = 1 секунда
    cache.put("ttl_test", {42}, 1);
    
    // Проверяем, что элемент есть
    auto v = cache.get("ttl_test");
    assert(v && (*v)[0] == 42);
    
    // Ждем 2 секунды для истечения TTL
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Принудительно запускаем очистку
    cache.cleanupSync();
    
    // Проверяем, что элемент истек
    auto expired = cache.get("ttl_test");
    assert(!expired);
    
    std::cout << "[OK] DynamicCache TTL test\n";
}

int main() {
    try {
        smokeTestDynamicCache();
        stressTestDynamicCache();
        testDynamicCacheTTL();
        std::cout << "All DynamicCache tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 