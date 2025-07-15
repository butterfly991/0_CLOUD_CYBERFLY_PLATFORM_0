#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace cloud {
namespace core {
namespace balancer {

// EnergyController — управление энергопотреблением, поддержка стратегий энергосбережения и мониторинга
class EnergyController {
public:
    EnergyController();
    ~EnergyController();
    bool initialize(); // Инициализация
    void shutdown();   // Завершение работы
    void setPowerLimit(double watts); // Лимит мощности
    double getPowerLimit() const;     // Получить лимит
    double getCurrentPower() const;   // Текущее энергопотребление
    void updateMetrics();             // Обновить метрики
    void enableDynamicScaling(bool enable); // Динамическое масштабирование
    void setEnergyPolicy(const std::string& policy); // Политика энергопотребления
    std::string getEnergyPolicy() const; // Получить политику
private:
    double powerLimit; // Лимит мощности
    double currentPower; // Текущее энергопотребление
    std::string energyPolicy; // Политика
    bool dynamicScalingEnabled; // Динамическое масштабирование
    mutable std::mutex mutex_;
};

} // namespace balancer
} // namespace core
} // namespace cloud
