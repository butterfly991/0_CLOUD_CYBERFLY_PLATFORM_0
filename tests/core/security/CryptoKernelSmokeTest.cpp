#include <cassert>
#include <iostream>
#include <memory>
#include "core/security/CryptoKernel.hpp"

void smokeTestCryptoKernel() {
    std::cout << "Testing CryptoKernel basic operations...\n";
    
    cloud::core::security::CryptoKernel kernel("crypto_test");
    assert(kernel.initialize());
    assert(kernel.getId() == "crypto_test");
    
    kernel.shutdown();
    std::cout << "[OK] CryptoKernel smoke test\n";
}

void testCryptoKernelExecute() {
    std::cout << "Testing CryptoKernel execute operations...\n";
    
    cloud::core::security::CryptoKernel kernel("execute_test");
    assert(kernel.initialize());
    
    // Тестовые данные для шифрования
    std::vector<uint8_t> inputData = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::vector<uint8_t> result;
    
    // Выполняем криптографическую операцию
    bool success = kernel.execute(inputData, result);
    assert(success);
    
    // Проверяем, что результат не пустой
    assert(!result.empty());
    
    kernel.shutdown();
    std::cout << "[OK] CryptoKernel execute test\n";
}

void testCryptoKernelMetrics() {
    std::cout << "Testing CryptoKernel metrics...\n";
    
    cloud::core::security::CryptoKernel kernel("metrics_test");
    assert(kernel.initialize());
    
    // Обновляем метрики
    kernel.updateMetrics();
    
    // Проверяем ID
    assert(kernel.getId() == "metrics_test");
    
    kernel.shutdown();
    std::cout << "[OK] CryptoKernel metrics test\n";
}

void testCryptoKernelMultipleOperations() {
    std::cout << "Testing CryptoKernel multiple operations...\n";
    
    cloud::core::security::CryptoKernel kernel("multi_test");
    assert(kernel.initialize());
    
    // Выполняем несколько криптографических операций
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> inputData = {static_cast<uint8_t>(i), static_cast<uint8_t>(i+1), 
                                         static_cast<uint8_t>(i+2), static_cast<uint8_t>(i+3)};
        std::vector<uint8_t> result;
        
        bool success = kernel.execute(inputData, result);
        assert(success);
        assert(!result.empty());
    }
    
    kernel.shutdown();
    std::cout << "[OK] CryptoKernel multiple operations test\n";
}

void testCryptoKernelLargeData() {
    std::cout << "Testing CryptoKernel large data processing...\n";
    
    cloud::core::security::CryptoKernel kernel("large_data_test");
    assert(kernel.initialize());
    
    // Создаем большие данные
    std::vector<uint8_t> largeInput(1024); // 1KB данных
    for (size_t i = 0; i < largeInput.size(); ++i) {
        largeInput[i] = static_cast<uint8_t>(i % 256);
    }
    
    std::vector<uint8_t> result;
    bool success = kernel.execute(largeInput, result);
    assert(success);
    assert(!result.empty());
    
    kernel.shutdown();
    std::cout << "[OK] CryptoKernel large data test\n";
}

void testCryptoKernelStressTest() {
    std::cout << "Testing CryptoKernel stress operations...\n";
    
    cloud::core::security::CryptoKernel kernel("stress_test");
    assert(kernel.initialize());
    
    // Выполняем много криптографических операций
    const int numOperations = 100;
    for (int i = 0; i < numOperations; ++i) {
        std::vector<uint8_t> inputData(64); // 64 байта данных
        for (size_t j = 0; j < inputData.size(); ++j) {
            inputData[j] = static_cast<uint8_t>((i + j) % 256);
        }
        
        std::vector<uint8_t> result;
        bool success = kernel.execute(inputData, result);
        assert(success);
        assert(!result.empty());
    }
    
    kernel.shutdown();
    std::cout << "[OK] CryptoKernel stress test\n";
}

void testCryptoKernelReinitialization() {
    std::cout << "Testing CryptoKernel reinitialization...\n";
    
    cloud::core::security::CryptoKernel kernel("reinit_test");
    
    // Инициализируем
    assert(kernel.initialize());
    assert(kernel.getId() == "reinit_test");
    
    // Завершаем работу
    kernel.shutdown();
    
    // Повторно инициализируем
    assert(kernel.initialize());
    assert(kernel.getId() == "reinit_test");
    
    // Тестируем операцию после повторной инициализации
    std::vector<uint8_t> inputData = {1, 2, 3, 4, 5};
    std::vector<uint8_t> result;
    bool success = kernel.execute(inputData, result);
    assert(success);
    
    kernel.shutdown();
    std::cout << "[OK] CryptoKernel reinitialization test\n";
}

int main() {
    try {
        smokeTestCryptoKernel();
        testCryptoKernelExecute();
        testCryptoKernelMetrics();
        testCryptoKernelMultipleOperations();
        testCryptoKernelLargeData();
        testCryptoKernelStressTest();
        testCryptoKernelReinitialization();
        std::cout << "All CryptoKernel tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "CryptoKernel test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "CryptoKernel test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 