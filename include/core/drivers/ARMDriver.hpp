#pragma once
#include <string>
#include <vector>
#include <optional>
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace drivers {

// ARMDriver — драйвер для ARM-платформ (Apple Silicon M1-M4), поддержка NEON/AMX, аппаратное ускорение вычислений и копирования данных. Используется для оптимизации вычислений на Mac и облачных ARM-серверах.
class ARMDriver {
public:
    ARMDriver();
    virtual ~ARMDriver();

    // Инициализация драйвера (детектирование возможностей платформы).
    virtual bool initialize();
    // Завершение работы драйвера, освобождение ресурсов.
    virtual void shutdown();

    // Проверка поддержки аппаратных ускорителей (NEON, AMX, SVE, Neural Engine).
    bool isNeonSupported() const;
    bool isAMXSupported() const;
    bool isSVEAvailable() const;
    bool isNeuralEngineAvailable() const;

    // Получение информации о платформе (строка: Apple Silicon, Linux ARM и др.).
    std::string getPlatformInfo() const;

    // Аппаратно-ускоренное копирование памяти (NEON/AMX, если доступно).
    virtual bool accelerateCopy(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
    // Аппаратно-ускоренное сложение массивов (пример).
    virtual bool accelerateAdd(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& result);
    // Аппаратно-ускоренное умножение массивов (пример).
    virtual bool accelerateMul(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& result);
    // Кастомные ускоренные операции (расширяемый интерфейс).
    virtual bool customAccelerate(const std::string& op, const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

protected:
    void detectCapabilities(); // Внутренний метод: определение возможностей платформы
    bool neonSupported = false;
    bool amxSupported = false;
    bool sveSupported = false;
    bool neuralEngineSupported = false;
    std::string platformInfo;
};

} // namespace drivers
} // namespace core
} // namespace cloud
