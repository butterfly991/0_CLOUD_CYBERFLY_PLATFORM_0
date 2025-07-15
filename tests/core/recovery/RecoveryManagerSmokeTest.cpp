#include <cassert>
#include <iostream>
#include <memory>
#include "core/recovery/RecoveryManager.hpp"

void smokeTestRecoveryManager() {
    std::cout << "Testing RecoveryManager basic operations...\n";
    
    cloud::core::recovery::config::RecoveryPointConfig pointConfig{1024 * 1024, false, "./recovery_points/test", std::chrono::seconds(3600)};
    cloud::core::recovery::RecoveryConfig config{10, std::chrono::seconds(60), true, true, pointConfig, "./logs/recovery.log", 1024 * 1024, 3};
    cloud::core::recovery::RecoveryManager manager(config);
    assert(manager.initialize());
    
    // Проверяем начальное состояние
    assert(!manager.isRecoveryInProgress());
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager smoke test\n";
}

void testRecoveryManagerCreateRecoveryPoint() {
    std::cout << "Testing RecoveryManager create recovery point...\n";
    
    cloud::core::recovery::RecoveryConfig config;
    config.maxRecoveryPoints = 5;
    config.checkpointInterval = std::chrono::seconds(10);
    config.enableAutoRecovery = false;
    config.enableStateValidation = true;
    config.pointConfig.maxSize = 1024 * 1024; // 1MB
    config.pointConfig.enableCompression = false;
    config.pointConfig.storagePath = "./recovery_points";
    config.pointConfig.retentionPeriod = std::chrono::seconds(1800); // 30 minutes
    
    cloud::core::recovery::RecoveryManager manager(config);
    assert(manager.initialize());
    
    // Создаем точку восстановления
    std::string pointId = manager.createRecoveryPoint();
    assert(!pointId.empty());
    
    // Проверяем метрики
    auto metrics = manager.getMetrics();
    assert(metrics.totalPoints > 0);
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager create recovery point test\n";
}

void testRecoveryManagerRestoreFromPoint() {
    std::cout << "Testing RecoveryManager restore from point (full integration)...\n";
    
    struct TestState {
        int a;
        std::string b;
        std::vector<uint8_t> c;
        
        bool operator==(const TestState& other) const {
            return a == other.a && b == other.b && c == other.c;
        }
    };
    
    cloud::core::recovery::RecoveryConfig config;
    config.maxRecoveryPoints = 3;
    config.checkpointInterval = std::chrono::seconds(5);
    config.enableAutoRecovery = false;
    config.enableStateValidation = true;
    config.pointConfig.maxSize = 1024 * 1024; // 1MB
    config.pointConfig.enableCompression = false;
    config.pointConfig.storagePath = "./recovery_points";
    config.pointConfig.retentionPeriod = std::chrono::seconds(900); // 15 minutes
    
    cloud::core::recovery::RecoveryManager manager(config);
    assert(manager.initialize());
    
    // Исходное состояние
    TestState originalState;
    originalState.a = 42;
    originalState.b = "test_string";
    originalState.c = {1,2,3,4,5,6,7,8,9,10};
    
    // Переменная для восстановленного состояния с синхронизацией
    std::atomic<bool> restoreCompleted{false};
    std::atomic<bool> restoreSuccess{false};
    TestState restoredState;
    std::mutex restoreMutex;
    
    // Устанавливаем callback для захвата состояния
    manager.setStateCaptureCallback([&originalState]() -> std::vector<uint8_t> {
        // Сериализуем состояние в байты
        std::vector<uint8_t> serialized;
        serialized.reserve(sizeof(int) + originalState.b.size() + originalState.c.size() + 16);
        
        // Добавляем размер int
        auto intBytes = reinterpret_cast<const uint8_t*>(&originalState.a);
        serialized.insert(serialized.end(), intBytes, intBytes + sizeof(int));
        
        // Добавляем размер строки и саму строку
        size_t strSize = originalState.b.size();
        auto strSizeBytes = reinterpret_cast<const uint8_t*>(&strSize);
        serialized.insert(serialized.end(), strSizeBytes, strSizeBytes + sizeof(size_t));
        serialized.insert(serialized.end(), originalState.b.begin(), originalState.b.end());
        
        // Добавляем размер вектора и сам вектор
        size_t vecSize = originalState.c.size();
        auto vecSizeBytes = reinterpret_cast<const uint8_t*>(&vecSize);
        serialized.insert(serialized.end(), vecSizeBytes, vecSizeBytes + sizeof(size_t));
        serialized.insert(serialized.end(), originalState.c.begin(), originalState.c.end());
        
        return serialized;
    });
    
    // Устанавливаем callback для восстановления состояния
    manager.setStateRestoreCallback([&restoredState, &restoreCompleted, &restoreSuccess, &restoreMutex](const std::vector<uint8_t>& data) -> bool {
        try {
            std::lock_guard<std::mutex> lock(restoreMutex);
            
            if (data.size() < sizeof(int) + sizeof(size_t)) {
                spdlog::error("RecoveryManager test: недостаточно данных для восстановления");
                restoreSuccess.store(false);
                restoreCompleted.store(true);
                return false;
            }
            
            size_t offset = 0;
            
            // Восстанавливаем int
            std::memcpy(&restoredState.a, data.data() + offset, sizeof(int));
            offset += sizeof(int);
            
            // Восстанавливаем размер строки
            size_t strSize;
            std::memcpy(&strSize, data.data() + offset, sizeof(size_t));
            offset += sizeof(size_t);
            
            if (offset + strSize > data.size()) {
                spdlog::error("RecoveryManager test: некорректный размер строки");
                restoreSuccess.store(false);
                restoreCompleted.store(true);
                return false;
            }
            
            // Восстанавливаем строку
            restoredState.b.assign(data.begin() + offset, data.begin() + offset + strSize);
            offset += strSize;
            
            // Восстанавливаем размер вектора
            if (offset + sizeof(size_t) > data.size()) {
                spdlog::error("RecoveryManager test: недостаточно данных для размера вектора");
                restoreSuccess.store(false);
                restoreCompleted.store(true);
                return false;
            }
            
            size_t vecSize;
            std::memcpy(&vecSize, data.data() + offset, sizeof(size_t));
            offset += sizeof(size_t);
            
            if (offset + vecSize > data.size()) {
                spdlog::error("RecoveryManager test: некорректный размер вектора");
                restoreSuccess.store(false);
                restoreCompleted.store(true);
                return false;
            }
            
            // Восстанавливаем вектор
            restoredState.c.assign(data.begin() + offset, data.begin() + offset + vecSize);
            
            spdlog::info("RecoveryManager test: состояние успешно восстановлено: a={}, b='{}', c.size={}", 
                        restoredState.a, restoredState.b, restoredState.c.size());
            
            restoreSuccess.store(true);
            restoreCompleted.store(true);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("RecoveryManager test: исключение при восстановлении: {}", e.what());
            restoreSuccess.store(false);
            restoreCompleted.store(true);
            return false;
        }
    });
    
    // Создаем точку восстановления
    std::string pointId = manager.createRecoveryPoint();
    assert(!pointId.empty());
    
    // Модифицируем исходное состояние (имитируем "порчу")
    originalState.a = 0;
    originalState.b = "corrupted";
    originalState.c.clear();
    
    // Восстанавливаемся из точки
    bool success = manager.restoreFromPoint(pointId);
    assert(success);
    
    // Ждем завершения восстановления с таймаутом
    auto startWait = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    
    while (!restoreCompleted.load() && 
           std::chrono::steady_clock::now() - startWait < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (!restoreCompleted.load()) {
        spdlog::error("RecoveryManager test: таймаут ожидания восстановления");
        assert(false && "Timeout waiting for restore completion");
    }
    
    assert(restoreSuccess.load() && "Restore operation failed");
    
    // Проверяем, что состояние восстановлено корректно
    {
        std::lock_guard<std::mutex> lock(restoreMutex);
        assert(restoredState.a == 42);
        assert(restoredState.b == "test_string");
        assert(restoredState.c == std::vector<uint8_t>({1,2,3,4,5,6,7,8,9,10}));
    }
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager restore from point (full integration) test\n";
}

void testRecoveryManagerDeleteRecoveryPoint() {
    std::cout << "Testing RecoveryManager delete recovery point...\n";
    
    cloud::core::recovery::RecoveryConfig config;
    config.maxRecoveryPoints = 5;
    config.checkpointInterval = std::chrono::seconds(10);
    config.enableAutoRecovery = false;
    config.enableStateValidation = true;
    config.pointConfig.maxSize = 1024 * 1024; // 1MB
    config.pointConfig.enableCompression = false;
    config.pointConfig.storagePath = "./recovery_points";
    config.pointConfig.retentionPeriod = std::chrono::seconds(1800); // 30 minutes
    
    cloud::core::recovery::RecoveryManager manager(config);
    assert(manager.initialize());
    
    // Создаем точку восстановления
    std::string pointId = manager.createRecoveryPoint();
    assert(!pointId.empty());
    
    // Получаем начальные метрики
    auto initialMetrics = manager.getMetrics();
    assert(initialMetrics.totalPoints > 0);
    
    // Удаляем точку восстановления
    manager.deleteRecoveryPoint(pointId);
    
    // Проверяем, что точка удалена
    auto finalMetrics = manager.getMetrics();
    assert(finalMetrics.totalPoints < initialMetrics.totalPoints);
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager delete recovery point test\n";
}

void testRecoveryManagerStateValidation() {
    std::cout << "Testing RecoveryManager state validation...\n";
    
    cloud::core::recovery::RecoveryConfig config;
    config.maxRecoveryPoints = 3;
    config.checkpointInterval = std::chrono::seconds(5);
    config.enableAutoRecovery = false;
    config.enableStateValidation = true;
    config.pointConfig.maxSize = 1024 * 1024; // 1MB
    config.pointConfig.enableCompression = false;
    config.pointConfig.storagePath = "./recovery_points";
    config.pointConfig.retentionPeriod = std::chrono::seconds(900); // 15 minutes
    
    cloud::core::recovery::RecoveryManager manager(config);
    assert(manager.initialize());
    
    // Создаем тестовые данные состояния
    std::vector<uint8_t> testState = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    // Валидируем состояние
    bool isValid = manager.validateState(testState);
    assert(isValid);
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager state validation test\n";
}

void testRecoveryManagerConfiguration() {
    std::cout << "Testing RecoveryManager configuration management...\n";
    
    cloud::core::recovery::RecoveryConfig initialConfig;
    initialConfig.maxRecoveryPoints = 5;
    initialConfig.checkpointInterval = std::chrono::seconds(30);
    initialConfig.enableAutoRecovery = false;
    initialConfig.enableStateValidation = true;
    initialConfig.pointConfig.maxSize = 1024 * 1024; // 1MB
    initialConfig.pointConfig.enableCompression = false;
    initialConfig.pointConfig.storagePath = "./recovery_points";
    initialConfig.pointConfig.retentionPeriod = std::chrono::seconds(1800); // 30 minutes
    
    cloud::core::recovery::RecoveryManager manager(initialConfig);
    assert(manager.initialize());
    
    // Проверяем текущую конфигурацию
    auto currentConfig = manager.getConfiguration();
    assert(currentConfig.maxRecoveryPoints == 5);
    assert(currentConfig.checkpointInterval.count() == 30);
    assert(currentConfig.enableAutoRecovery == false);
    assert(currentConfig.enableStateValidation == true);
    
    // Устанавливаем новую конфигурацию
    cloud::core::recovery::RecoveryConfig newConfig;
    newConfig.maxRecoveryPoints = 10;
    newConfig.checkpointInterval = std::chrono::seconds(60);
    newConfig.enableAutoRecovery = true;
    newConfig.enableStateValidation = false;
    newConfig.pointConfig.maxSize = 2 * 1024 * 1024; // 2MB
    newConfig.pointConfig.enableCompression = true;
    newConfig.pointConfig.storagePath = "./new_recovery_points";
    newConfig.pointConfig.retentionPeriod = std::chrono::seconds(3600); // 1 hour
    
    manager.setConfiguration(newConfig);
    
    // Проверяем обновленную конфигурацию
    auto updatedConfig = manager.getConfiguration();
    assert(updatedConfig.maxRecoveryPoints == 10);
    assert(updatedConfig.checkpointInterval.count() == 60);
    assert(updatedConfig.enableAutoRecovery == true);
    assert(updatedConfig.enableStateValidation == false);
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager configuration test\n";
}

void testRecoveryManagerMetrics() {
    std::cout << "Testing RecoveryManager metrics collection...\n";
    
    cloud::core::recovery::RecoveryConfig config;
    config.maxRecoveryPoints = 5;
    config.checkpointInterval = std::chrono::seconds(10);
    config.enableAutoRecovery = false;
    config.enableStateValidation = true;
    config.pointConfig.maxSize = 1024 * 1024; // 1MB
    config.pointConfig.enableCompression = false;
    config.pointConfig.storagePath = "./recovery_points";
    config.pointConfig.retentionPeriod = std::chrono::seconds(1800); // 30 minutes
    
    cloud::core::recovery::RecoveryManager manager(config);
    assert(manager.initialize());
    
    // Создаем несколько точек восстановления
    for (int i = 0; i < 3; ++i) {
        std::string pointId = manager.createRecoveryPoint();
        assert(!pointId.empty());
    }
    
    // Получаем метрики
    auto metrics = manager.getMetrics();
    assert(metrics.totalPoints >= 3);
    assert(metrics.successfulRecoveries >= 0);
    assert(metrics.failedRecoveries >= 0);
    assert(metrics.averageRecoveryTime >= 0.0);
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager metrics test\n";
}

void testRecoveryManagerStressTest() {
    std::cout << "Testing RecoveryManager stress operations...\n";
    
    cloud::core::recovery::RecoveryConfig config;
    config.maxRecoveryPoints = 20;
    config.checkpointInterval = std::chrono::seconds(5);
    config.enableAutoRecovery = false;
    config.enableStateValidation = true;
    config.pointConfig.maxSize = 1024 * 1024; // 1MB
    config.pointConfig.enableCompression = false;
    config.pointConfig.storagePath = "./recovery_points";
    config.pointConfig.retentionPeriod = std::chrono::seconds(900); // 15 minutes
    
    cloud::core::recovery::RecoveryManager manager(config);
    assert(manager.initialize());
    
    // Создаем много точек восстановления
    std::vector<std::string> pointIds;
    const int numPoints = 10;
    
    for (int i = 0; i < numPoints; ++i) {
        std::string pointId = manager.createRecoveryPoint();
        assert(!pointId.empty());
        pointIds.push_back(pointId);
    }
    
    // Проверяем метрики
    auto metrics = manager.getMetrics();
    assert(metrics.totalPoints >= numPoints);
    
    // Удаляем некоторые точки
    for (int i = 0; i < 5; ++i) {
        manager.deleteRecoveryPoint(pointIds[i]);
    }
    
    manager.shutdown();
    std::cout << "[OK] RecoveryManager stress test\n";
}

int main() {
    try {
        smokeTestRecoveryManager();
        testRecoveryManagerCreateRecoveryPoint();
        testRecoveryManagerRestoreFromPoint();
        testRecoveryManagerDeleteRecoveryPoint();
        testRecoveryManagerStateValidation();
        testRecoveryManagerConfiguration();
        testRecoveryManagerMetrics();
        testRecoveryManagerStressTest();
        std::cout << "All RecoveryManager tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "RecoveryManager test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "RecoveryManager test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 