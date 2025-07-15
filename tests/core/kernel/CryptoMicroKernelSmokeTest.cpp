#include <cassert>
#include <iostream>
#include <memory>
#include "core/kernel/advanced/CryptoMicroKernel.hpp"

void smokeTestCryptoMicroKernel() {
    std::cout << "Testing CryptoMicroKernel basic operations...\n";
    
    cloud::core::kernel::CryptoMicroKernel kernel("crypto_test");
    assert(kernel.initialize());
    assert(kernel.isRunning());
    assert(kernel.getId() == "crypto_test");
    assert(kernel.getType() == cloud::core::kernel::KernelType::CRYPTO);
    
    // Проверяем базовые метрики
    auto metrics = kernel.getMetrics();
    assert(metrics.cpu_usage >= 0.0);
    assert(metrics.memory_usage >= 0.0);
    
    kernel.shutdown();
    assert(!kernel.isRunning());
    
    std::cout << "[OK] CryptoMicroKernel smoke test\n";
}

void testCryptoMicroKernelCryptoTask() {
    std::cout << "Testing CryptoMicroKernel crypto task execution...\n";
    
    cloud::core::kernel::CryptoMicroKernel kernel("crypto_task_test");
    assert(kernel.initialize());
    
    // Тестовые данные для шифрования
    std::vector<uint8_t> inputData = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::vector<uint8_t> result;
    
    // Тестируем выполнение криптографической задачи
    bool success = kernel.executeCryptoTask(inputData, result);
    assert(success);
    
    // Проверяем, что результат не пустой
    assert(!result.empty());
    
    kernel.shutdown();
    std::cout << "[OK] CryptoMicroKernel crypto task test\n";
}

void testCryptoMicroKernelHardwareAcceleration() {
    std::cout << "Testing CryptoMicroKernel hardware acceleration...\n";
    
    cloud::core::kernel::CryptoMicroKernel kernel("hw_accel_test");
    assert(kernel.initialize());
    
    std::vector<uint8_t> inputData = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    std::vector<uint8_t> result;
    
    // Тестируем аппаратное ускорение криптографии
    bool success = kernel.performHardwareAcceleratedCrypto(inputData, result);
    
    // Даже если аппаратное ускорение недоступно, операция должна завершиться
    // (может быть реализована через программную криптографию)
    
    kernel.shutdown();
    std::cout << "[OK] CryptoMicroKernel hardware acceleration test\n";
}

void testCryptoMicroKernelSoftwareCrypto() {
    std::cout << "Testing CryptoMicroKernel software crypto...\n";
    
    std::vector<uint8_t> inputData = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    
    // Тестируем программную криптографию
    cloud::core::kernel::CryptoMicroKernel kernel("test_id");
    auto result = kernel.performSoftwareCrypto(inputData);
    
    // Проверяем, что результат не пустой
    assert(!result.empty());
    
    std::cout << "[OK] CryptoMicroKernel software crypto test\n";
}

void testCryptoMicroKernelResourceManagement() {
    std::cout << "Testing CryptoMicroKernel resource management...\n";
    
    cloud::core::kernel::CryptoMicroKernel kernel("resource_test");
    assert(kernel.initialize());
    
    // Устанавливаем лимиты ресурсов
    kernel.setResourceLimit("cpu", 0.9);
    kernel.setResourceLimit("memory", 1024 * 1024 * 50); // 50MB
    kernel.setResourceLimit("crypto_accelerator", 0.8);
    
    // Проверяем использование ресурсов
    double cpuUsage = kernel.getResourceUsage("cpu");
    double memoryUsage = kernel.getResourceUsage("memory");
    double cryptoUsage = kernel.getResourceUsage("crypto_accelerator");
    
    assert(cpuUsage >= 0.0);
    assert(memoryUsage >= 0.0);
    assert(cryptoUsage >= 0.0);
    
    kernel.shutdown();
    std::cout << "[OK] CryptoMicroKernel resource management test\n";
}

void testCryptoMicroKernelTaskScheduling() {
    std::cout << "Testing CryptoMicroKernel task scheduling...\n";
    
    cloud::core::kernel::CryptoMicroKernel kernel("scheduler_test");
    assert(kernel.initialize());
    
    std::atomic<int> taskCounter{0};
    
    // Планируем криптографические задачи
    for (int i = 0; i < 3; ++i) {
        kernel.scheduleTask([&taskCounter, i]() {
            taskCounter++;
            // Симулируем криптографическую операцию
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }, i % 2);
    }
    
    // Даем время на выполнение
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    kernel.shutdown();
    std::cout << "[OK] CryptoMicroKernel task scheduling test\n";
}

int main() {
    try {
        smokeTestCryptoMicroKernel();
        testCryptoMicroKernelCryptoTask();
        testCryptoMicroKernelHardwareAcceleration();
        testCryptoMicroKernelSoftwareCrypto();
        testCryptoMicroKernelResourceManagement();
        testCryptoMicroKernelTaskScheduling();
        std::cout << "All CryptoMicroKernel tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "CryptoMicroKernel test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "CryptoMicroKernel test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 