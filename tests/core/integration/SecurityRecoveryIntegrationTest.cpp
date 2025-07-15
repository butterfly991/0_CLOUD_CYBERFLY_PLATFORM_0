#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include <filesystem>
#include "core/security/SecurityManager.hpp"
#include "core/security/CryptoKernel.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/kernel/base/MicroKernel.hpp"

void testSecurityRecoveryBasicIntegration() {
    std::cout << "Testing Security-Recovery basic integration...\n";
    
    // Создаем менеджер безопасности
    auto securityManager = std::make_shared<cloud::core::security::SecurityManager>();
    assert(securityManager->initialize());
    
    // Создаем криптографическое ядро
    auto cryptoKernel = std::make_shared<cloud::core::security::CryptoKernel>("security_crypto");
    assert(cryptoKernel->initialize());
    
    // Создаем менеджер восстановления
    cloud::core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 5;
    recoveryConfig.checkpointInterval = std::chrono::seconds(30);
    recoveryConfig.enableAutoRecovery = true;
    recoveryConfig.enableStateValidation = true;
    recoveryConfig.pointConfig.maxSize = 1024 * 1024; // 1MB
    recoveryConfig.pointConfig.enableCompression = false;
    recoveryConfig.pointConfig.storagePath = "./recovery_points";
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::seconds(1800); // 30 minutes
    
    // Создаем директорию для recovery points
    std::filesystem::create_directories("./recovery_points");
    
    auto recoveryManager = std::make_shared<cloud::core::recovery::RecoveryManager>(recoveryConfig);
    assert(recoveryManager->initialize());
    
    // Устанавливаем политику безопасности
    securityManager->setPolicy("secure");
    assert(securityManager->getPolicy() == "secure");
    
    // Выполняем криптографическую операцию
    std::vector<uint8_t> inputData = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint8_t> result;
    bool cryptoSuccess = cryptoKernel->execute(inputData, result);
    assert(cryptoSuccess);
    
    // Создаем точку восстановления
    std::string recoveryPointId = recoveryManager->createRecoveryPoint();
    assert(!recoveryPointId.empty());
    
    // Аудит события безопасности
    securityManager->auditEvent("crypto_operation", "encryption_completed");
    securityManager->auditEvent("recovery_point_created", recoveryPointId);
    
    // Проверяем метрики восстановления
    auto recoveryMetrics = recoveryManager->getMetrics();
    assert(recoveryMetrics.totalPoints > 0);
    
    // Завершаем работу
    securityManager->shutdown();
    cryptoKernel->shutdown();
    recoveryManager->shutdown();
    
    std::cout << "[OK] Security-Recovery basic integration test\n";
}

void testSecurityRecoveryAdvancedIntegration() {
    std::cout << "Testing Security-Recovery advanced integration...\n";
    
    // Создаем компоненты безопасности
    auto securityManager = std::make_shared<cloud::core::security::SecurityManager>();
    auto cryptoKernel = std::make_shared<cloud::core::security::CryptoKernel>("advanced_crypto");
    
    assert(securityManager->initialize());
    assert(cryptoKernel->initialize());
    
    // Создаем менеджер восстановления с расширенной конфигурацией
    cloud::core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 10;
    recoveryConfig.checkpointInterval = std::chrono::seconds(15);
    recoveryConfig.enableAutoRecovery = true;
    recoveryConfig.enableStateValidation = true;
    recoveryConfig.pointConfig.maxSize = 2 * 1024 * 1024; // 2MB
    recoveryConfig.pointConfig.enableCompression = true;
    recoveryConfig.pointConfig.storagePath = "./secure_recovery_points";
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::seconds(3600); // 1 hour
    
    // Создаем директорию для recovery points
    std::filesystem::create_directories("./secure_recovery_points");
    
    auto recoveryManager = std::make_shared<cloud::core::recovery::RecoveryManager>(recoveryConfig);
    assert(recoveryManager->initialize());
    
    // Устанавливаем строгую политику безопасности
    securityManager->setPolicy("strict");
    assert(securityManager->checkPolicy("strict"));
    
    // Выполняем несколько криптографических операций
    std::vector<std::string> recoveryPointIds;
    for (int i = 0; i < 3; ++i) {
        std::vector<uint8_t> inputData(100, i);
        std::vector<uint8_t> result;
        
        bool cryptoSuccess = cryptoKernel->execute(inputData, result);
        assert(cryptoSuccess);
        
        // Создаем точку восстановления после каждой операции
        std::string pointId = recoveryManager->createRecoveryPoint();
        assert(!pointId.empty());
        recoveryPointIds.push_back(pointId);
        
        // Аудит операции
        securityManager->auditEvent("crypto_operation", "operation_" + std::to_string(i));
        securityManager->auditEvent("recovery_point", pointId);
    }
    
    // Проверяем метрики
    auto recoveryMetrics = recoveryManager->getMetrics();
    assert(recoveryMetrics.totalPoints >= 3);
    
    // Восстанавливаемся из первой точки
    if (!recoveryPointIds.empty()) {
        bool restoreSuccess = recoveryManager->restoreFromPoint(recoveryPointIds[0]);
        assert(restoreSuccess);
    }
    
    // Завершаем работу
    securityManager->shutdown();
    cryptoKernel->shutdown();
    recoveryManager->shutdown();
    
    std::cout << "[OK] Security-Recovery advanced integration test\n";
}

void testSecurityRecoveryErrorHandling() {
    std::cout << "Testing Security-Recovery error handling...\n";
    
    // Создаем компоненты
    auto securityManager = std::make_shared<cloud::core::security::SecurityManager>();
    auto cryptoKernel = std::make_shared<cloud::core::security::CryptoKernel>("error_crypto");
    
    assert(securityManager->initialize());
    assert(cryptoKernel->initialize());
    
    // Создаем менеджер восстановления
    cloud::core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 3;
    recoveryConfig.checkpointInterval = std::chrono::seconds(10);
    recoveryConfig.enableAutoRecovery = false;
    recoveryConfig.enableStateValidation = true;
    recoveryConfig.pointConfig.maxSize = 1024 * 1024; // 1MB
    recoveryConfig.pointConfig.enableCompression = false;
    recoveryConfig.pointConfig.storagePath = "./error_recovery_points";
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::seconds(900); // 15 minutes
    
    // Создаем директорию для recovery points
    std::filesystem::create_directories("./error_recovery_points");
    
    auto recoveryManager = std::make_shared<cloud::core::recovery::RecoveryManager>(recoveryConfig);
    assert(recoveryManager->initialize());
    
    // Тестируем обработку ошибок безопасности
    securityManager->setPolicy("strict");
    
    // Попытка восстановления из несуществующей точки
    bool restoreSuccess = recoveryManager->restoreFromPoint("non_existent_point");
    // Может вернуть false для несуществующей точки
    
    // Аудит ошибки
    securityManager->auditEvent("recovery_error", "non_existent_point");
    
    // Создаем валидную точку восстановления
    std::string validPointId = recoveryManager->createRecoveryPoint();
    assert(!validPointId.empty());
    
    // Восстанавливаемся из валидной точки
    bool validRestoreSuccess = recoveryManager->restoreFromPoint(validPointId);
    assert(validRestoreSuccess);
    
    // Завершаем работу
    securityManager->shutdown();
    cryptoKernel->shutdown();
    recoveryManager->shutdown();
    
    std::cout << "[OK] Security-Recovery error handling test\n";
}

void testSecurityRecoveryStressIntegration() {
    std::cout << "Testing Security-Recovery stress integration...\n";
    
    // Создаем компоненты
    auto securityManager = std::make_shared<cloud::core::security::SecurityManager>();
    auto cryptoKernel = std::make_shared<cloud::core::security::CryptoKernel>("stress_crypto");
    
    assert(securityManager->initialize());
    assert(cryptoKernel->initialize());
    
    // Создаем менеджер восстановления
    cloud::core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 20;
    recoveryConfig.checkpointInterval = std::chrono::seconds(5);
    recoveryConfig.enableAutoRecovery = true;
    recoveryConfig.enableStateValidation = true;
    recoveryConfig.pointConfig.maxSize = 1024 * 1024; // 1MB
    recoveryConfig.pointConfig.enableCompression = false;
    recoveryConfig.pointConfig.storagePath = "./stress_recovery_points";
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::seconds(1800); // 30 minutes
    
    // Создаем директорию для recovery points
    std::filesystem::create_directories("./stress_recovery_points");
    
    auto recoveryManager = std::make_shared<cloud::core::recovery::RecoveryManager>(recoveryConfig);
    assert(recoveryManager->initialize());
    
    // Устанавливаем политику безопасности
    securityManager->setPolicy("performance");
    
    // Выполняем много операций
    const int numOperations = 50;
    std::vector<std::string> pointIds;
    
    for (int i = 0; i < numOperations; ++i) {
        // Криптографическая операция
        std::vector<uint8_t> inputData(200, i % 256);
        std::vector<uint8_t> result;
        bool cryptoSuccess = cryptoKernel->execute(inputData, result);
        assert(cryptoSuccess);
        
        // Создание точки восстановления
        std::string pointId = recoveryManager->createRecoveryPoint();
        if (!pointId.empty()) {
            pointIds.push_back(pointId);
        }
        
        // Аудит
        securityManager->auditEvent("stress_operation", std::to_string(i));
    }
    
    // Проверяем метрики
    auto recoveryMetrics = recoveryManager->getMetrics();
    assert(recoveryMetrics.totalPoints > 0);
    
    // Восстанавливаемся из нескольких точек
    int successfulRestores = 0;
    for (const auto& pointId : pointIds) {
        if (recoveryManager->restoreFromPoint(pointId)) {
            successfulRestores++;
        }
    }
    
    assert(successfulRestores > 0);
    
    // Завершаем работу
    securityManager->shutdown();
    cryptoKernel->shutdown();
    recoveryManager->shutdown();
    
    std::cout << "[OK] Security-Recovery stress integration test\n";
}

void testSecurityRecoveryKernelIntegration() {
    std::cout << "Testing Security-Recovery-Kernel integration...\n";
    
    // Создаем компоненты безопасности
    auto securityManager = std::make_shared<cloud::core::security::SecurityManager>();
    auto cryptoKernel = std::make_shared<cloud::core::security::CryptoKernel>("kernel_crypto");
    
    assert(securityManager->initialize());
    assert(cryptoKernel->initialize());
    
    // Создаем обычное ядро
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("security_micro");
    assert(microKernel->initialize());
    
    // Создаем менеджер восстановления
    cloud::core::recovery::RecoveryConfig recoveryConfig;
    recoveryConfig.maxRecoveryPoints = 5;
    recoveryConfig.checkpointInterval = std::chrono::seconds(20);
    recoveryConfig.enableAutoRecovery = true;
    recoveryConfig.enableStateValidation = true;
    recoveryConfig.pointConfig.maxSize = 1024 * 1024; // 1MB
    recoveryConfig.pointConfig.enableCompression = false;
    recoveryConfig.pointConfig.storagePath = "./kernel_recovery_points";
    recoveryConfig.pointConfig.retentionPeriod = std::chrono::seconds(1200); // 20 minutes
    
    auto recoveryManager = std::make_shared<cloud::core::recovery::RecoveryManager>(recoveryConfig);
    assert(recoveryManager->initialize());
    
    // Устанавливаем политику безопасности
    securityManager->setPolicy("balanced");
    
    // Выполняем операции на ядре
    microKernel->scheduleTask([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }, 5);
    
    // Выполняем криптографическую операцию
    std::vector<uint8_t> inputData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<uint8_t> result;
    bool cryptoSuccess = cryptoKernel->execute(inputData, result);
    assert(cryptoSuccess);
    
    // Создаем точку восстановления
    std::string recoveryPointId = recoveryManager->createRecoveryPoint();
    assert(!recoveryPointId.empty());
    
    // Аудит всех операций
    securityManager->auditEvent("kernel_operation", "task_scheduled");
    securityManager->auditEvent("crypto_operation", "encryption_completed");
    securityManager->auditEvent("recovery_point", recoveryPointId);
    
    // Проверяем метрики всех компонентов
    auto kernelMetrics = microKernel->getMetrics();
    assert(kernelMetrics.cpu_usage >= 0.0);
    
    auto recoveryMetrics = recoveryManager->getMetrics();
    assert(recoveryMetrics.totalPoints > 0);
    
    // Завершаем работу
    securityManager->shutdown();
    cryptoKernel->shutdown();
    microKernel->shutdown();
    recoveryManager->shutdown();
    
    std::cout << "[OK] Security-Recovery-Kernel integration test\n";
}

int main() {
    try {
        testSecurityRecoveryBasicIntegration();
        testSecurityRecoveryAdvancedIntegration();
        testSecurityRecoveryErrorHandling();
        testSecurityRecoveryStressIntegration();
        testSecurityRecoveryKernelIntegration();
        std::cout << "All Security-Recovery integration tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Security-Recovery integration test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Security-Recovery integration test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 