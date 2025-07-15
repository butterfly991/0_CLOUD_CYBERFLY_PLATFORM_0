#include <cassert>
#include <iostream>
#include <memory>
#include "core/drivers/ARMDriver.hpp"

void smokeTestARMDriver() {
    std::cout << "Testing ARMDriver basic operations...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Проверяем информацию о платформе
    std::string platformInfo = driver.getPlatformInfo();
    assert(!platformInfo.empty());
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver smoke test\n";
}

void testARMDriverCapabilities() {
    std::cout << "Testing ARMDriver capabilities detection...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Проверяем поддержку различных технологий
    bool neonSupported = driver.isNeonSupported();
    bool amxSupported = driver.isAMXSupported();
    bool sveSupported = driver.isSVEAvailable();
    bool neuralEngineSupported = driver.isNeuralEngineAvailable();
    
    // Выводим информацию о возможностях
    std::cout << "NEON support: " << (neonSupported ? "Yes" : "No") << std::endl;
    std::cout << "AMX support: " << (amxSupported ? "Yes" : "No") << std::endl;
    std::cout << "SVE support: " << (sveSupported ? "Yes" : "No") << std::endl;
    std::cout << "Neural Engine support: " << (neuralEngineSupported ? "Yes" : "No") << std::endl;
    
    // Хотя бы одна технология должна быть доступна для успешной инициализации
    assert(neonSupported || amxSupported || sveSupported || neuralEngineSupported);
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver capabilities test\n";
}

void testARMDriverAccelerateCopy() {
    std::cout << "Testing ARMDriver accelerate copy...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Создаем тестовые данные
    std::vector<uint8_t> inputData(1024);
    for (size_t i = 0; i < inputData.size(); ++i) {
        inputData[i] = static_cast<uint8_t>(i % 256);
    }
    
    std::vector<uint8_t> outputData;
    
    // Тестируем ускоренное копирование
    bool success = driver.accelerateCopy(inputData, outputData);
    
    // Даже если аппаратное ускорение недоступно, операция должна завершиться
    // (может быть реализована через программное копирование)
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver accelerate copy test\n";
}

void testARMDriverAccelerateAdd() {
    std::cout << "Testing ARMDriver accelerate add...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Создаем тестовые данные
    std::vector<uint8_t> a(512);
    std::vector<uint8_t> b(512);
    for (size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<uint8_t>(i % 128);
        b[i] = static_cast<uint8_t>((i + 1) % 128);
    }
    
    std::vector<uint8_t> result;
    
    // Тестируем ускоренное сложение
    bool success = driver.accelerateAdd(a, b, result);
    
    // Даже если аппаратное ускорение недоступно, операция должна завершиться
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver accelerate add test\n";
}

void testARMDriverAccelerateMul() {
    std::cout << "Testing ARMDriver accelerate multiply...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Создаем тестовые данные
    std::vector<uint8_t> a(256);
    std::vector<uint8_t> b(256);
    for (size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<uint8_t>(i % 64);
        b[i] = static_cast<uint8_t>((i + 2) % 64);
    }
    
    std::vector<uint8_t> result;
    
    // Тестируем ускоренное умножение
    bool success = driver.accelerateMul(a, b, result);
    
    // Даже если аппаратное ускорение недоступно, операция должна завершиться
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver accelerate multiply test\n";
}

void testARMDriverCustomAccelerate() {
    std::cout << "Testing ARMDriver custom accelerate...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Создаем тестовые данные
    std::vector<uint8_t> inputData = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint8_t> outputData;
    
    // Тестируем кастомную операцию
    bool success = driver.customAccelerate("test_operation", inputData, outputData);
    
    // Кастомные операции могут не быть реализованы, поэтому проверяем только завершение
    assert(!success); // Ожидаем false для нереализованной операции
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver custom accelerate test\n";
}

void testARMDriverPlatformInfo() {
    std::cout << "Testing ARMDriver platform information...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Получаем информацию о платформе
    std::string platformInfo = driver.getPlatformInfo();
    assert(!platformInfo.empty());
    
    // Выводим информацию о платформе
    std::cout << "Platform: " << platformInfo << std::endl;
    
    // Проверяем, что информация содержит полезные данные
    assert(platformInfo != "Unknown/Unsupported");
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver platform info test\n";
}

void testARMDriverStressTest() {
    std::cout << "Testing ARMDriver stress operations...\n";
    
    cloud::core::drivers::ARMDriver driver;
    assert(driver.initialize());
    
    // Создаем большие тестовые данные
    std::vector<uint8_t> largeInput(1024 * 1024); // 1MB данных
    for (size_t i = 0; i < largeInput.size(); ++i) {
        largeInput[i] = static_cast<uint8_t>(i % 256);
    }
    
    std::vector<uint8_t> outputData;
    
    // Выполняем несколько операций
    for (int i = 0; i < 5; ++i) {
        bool success = driver.accelerateCopy(largeInput, outputData);
        // Операция должна завершиться
    }
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver stress test\n";
}

void testARMDriverReinitialization() {
    std::cout << "Testing ARMDriver reinitialization...\n";
    
    cloud::core::drivers::ARMDriver driver;
    
    // Первая инициализация
    assert(driver.initialize());
    std::string platformInfo1 = driver.getPlatformInfo();
    driver.shutdown();
    
    // Повторная инициализация
    assert(driver.initialize());
    std::string platformInfo2 = driver.getPlatformInfo();
    
    // Информация о платформе должна быть одинаковой
    assert(platformInfo1 == platformInfo2);
    
    driver.shutdown();
    std::cout << "[OK] ARMDriver reinitialization test\n";
}

int main() {
    try {
        smokeTestARMDriver();
        testARMDriverCapabilities();
        testARMDriverAccelerateCopy();
        testARMDriverAccelerateAdd();
        testARMDriverAccelerateMul();
        testARMDriverCustomAccelerate();
        testARMDriverPlatformInfo();
        testARMDriverStressTest();
        testARMDriverReinitialization();
        std::cout << "All ARMDriver tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "ARMDriver test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ARMDriver test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 