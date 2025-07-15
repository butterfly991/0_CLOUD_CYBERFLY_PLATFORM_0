#include <cassert>
#include <iostream>
#include <thread>
#include <memory>
#include "core/balancer/EnergyController.hpp"

void smokeTestEnergyController() {
    std::cout << "Testing EnergyController basic operations...\n";
    
    cloud::core::balancer::EnergyController controller;
    assert(controller.initialize());
    
    // Проверяем начальные значения
    assert(controller.getPowerLimit() > 0.0);
    assert(controller.getCurrentPower() >= 0.0);
    
    controller.shutdown();
    std::cout << "[OK] EnergyController smoke test\n";
}

void testEnergyControllerPowerLimits() {
    std::cout << "Testing EnergyController power limits...\n";
    
    cloud::core::balancer::EnergyController controller;
    assert(controller.initialize());
    
    // Устанавливаем лимит мощности
    double testLimit = 150.0; // 150 Watts
    controller.setPowerLimit(testLimit);
    assert(controller.getPowerLimit() == testLimit);
    
    // Устанавливаем другой лимит
    double newLimit = 200.0; // 200 Watts
    controller.setPowerLimit(newLimit);
    assert(controller.getPowerLimit() == newLimit);
    
    controller.shutdown();
    std::cout << "[OK] EnergyController power limits test\n";
}

void testEnergyControllerCurrentPower() {
    std::cout << "Testing EnergyController current power monitoring...\n";
    
    cloud::core::balancer::EnergyController controller;
    assert(controller.initialize());
    
    // Проверяем текущее энергопотребление
    double currentPower = controller.getCurrentPower();
    assert(currentPower >= 0.0);
    
    // Обновляем метрики
    controller.updateMetrics();
    
    // Проверяем снова
    double updatedPower = controller.getCurrentPower();
    assert(updatedPower >= 0.0);
    
    controller.shutdown();
    std::cout << "[OK] EnergyController current power test\n";
}

void testEnergyControllerDynamicScaling() {
    std::cout << "Testing EnergyController dynamic scaling...\n";
    
    cloud::core::balancer::EnergyController controller;
    assert(controller.initialize());
    
    // Включаем динамическое масштабирование
    controller.enableDynamicScaling(true);
    
    // Устанавливаем политику энергопотребления
    controller.setEnergyPolicy("performance");
    assert(controller.getEnergyPolicy() == "performance");
    
    // Меняем политику
    controller.setEnergyPolicy("efficiency");
    assert(controller.getEnergyPolicy() == "efficiency");
    
    // Выключаем динамическое масштабирование
    controller.enableDynamicScaling(false);
    
    controller.shutdown();
    std::cout << "[OK] EnergyController dynamic scaling test\n";
}

void testEnergyControllerEnergyPolicies() {
    std::cout << "Testing EnergyController energy policies...\n";
    
    cloud::core::balancer::EnergyController controller;
    assert(controller.initialize());
    
    // Тестируем различные политики
    std::vector<std::string> policies = {"default", "performance", "efficiency", "balanced"};
    
    for (const auto& policy : policies) {
        controller.setEnergyPolicy(policy);
        assert(controller.getEnergyPolicy() == policy);
    }
    
    controller.shutdown();
    std::cout << "[OK] EnergyController energy policies test\n";
}

void testEnergyControllerMetricsUpdate() {
    std::cout << "Testing EnergyController metrics update...\n";
    
    cloud::core::balancer::EnergyController controller;
    assert(controller.initialize());
    
    // Получаем начальные метрики
    double initialPower = controller.getCurrentPower();
    
    // Обновляем метрики несколько раз
    for (int i = 0; i < 5; ++i) {
        controller.updateMetrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Проверяем, что метрики обновляются
    double finalPower = controller.getCurrentPower();
    assert(finalPower >= 0.0);
    
    controller.shutdown();
    std::cout << "[OK] EnergyController metrics update test\n";
}

int main() {
    try {
        smokeTestEnergyController();
        testEnergyControllerPowerLimits();
        testEnergyControllerCurrentPower();
        testEnergyControllerDynamicScaling();
        testEnergyControllerEnergyPolicies();
        testEnergyControllerMetricsUpdate();
        std::cout << "All EnergyController tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "EnergyController test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "EnergyController test failed with unknown exception" << std::endl;
        return 1;
    }
    return 0;
} 