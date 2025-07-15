#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <locale>
#include <codecvt>
#include <random>
#include <ios>
#include <streambuf>
#include <shared_mutex>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "core/thread/ThreadPool.hpp"

#if defined(__APPLE__) && defined(__arm64__)
    #include <mach/mach.h>
    #include <sys/sysctl.h>
#endif

namespace cloud {
namespace core {
namespace recovery {

// Вложенные пространства имен для лучшей организации
namespace detail {
    class RecoveryLogger;
    class StateValidator;
    class CheckpointManager;
}

namespace config {
    struct RecoveryPointConfig {
        size_t maxSize;
        bool enableCompression;
        std::string storagePath;
        std::chrono::seconds retentionPeriod;
    };
}

namespace metrics {
    struct RecoveryMetrics {
        size_t totalPoints;
        size_t successfulRecoveries;
        size_t failedRecoveries;
        double averageRecoveryTime;
        std::chrono::steady_clock::time_point lastRecovery;
    };
}

// Улучшенная структура точки восстановления
struct RecoveryPoint {
    std::string id;
    std::chrono::steady_clock::time_point timestamp;
    std::vector<uint8_t> state;
    bool isConsistent;
    std::string checksum;
    size_t size;
    std::unordered_map<std::string, std::string> metadata;
    
    // Сериализация в JSON
    nlohmann::json toJson() const {
        // Сериализуем state как base64 строку
        std::string stateBase64;
        if (!state.empty()) {
            const std::string base64_chars = 
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz"
                "0123456789+/";
            
            std::string encoded;
            int val = 0, valb = -6;
            for (unsigned char c : state) {
                val = (val << 8) + c;
                valb += 8;
                while (valb >= 0) {
                    encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
            while (encoded.size() % 4) encoded.push_back('=');
            stateBase64 = encoded;
        }
        
        return {
            {"id", id},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()},
            {"state", stateBase64},
            {"size", size},
            {"isConsistent", isConsistent},
            {"checksum", checksum},
            {"metadata", metadata}
        };
    }
    
    // Десериализация из JSON
    static RecoveryPoint fromJson(const nlohmann::json& j) {
        RecoveryPoint point;
        point.id = j["id"];
        point.timestamp = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(j["timestamp"]));
        point.size = j["size"];
        point.isConsistent = j["isConsistent"];
        point.checksum = j["checksum"];
        point.metadata = j["metadata"].get<std::unordered_map<std::string, std::string>>();
        
        // Десериализуем state из base64 строки
        if (j.contains("state") && !j["state"].is_null()) {
            std::string stateBase64 = j["state"];
            if (!stateBase64.empty()) {
                const std::string base64_chars = 
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";
                
                std::vector<uint8_t> decoded;
                int val = 0, valb = -8;
                for (char c : stateBase64) {
                    if (c == '=') break;
                    auto it = base64_chars.find(c);
                    if (it == std::string::npos) continue;
                    val = (val << 6) + it;
                    valb += 6;
                    if (valb >= 0) {
                        decoded.push_back((val >> valb) & 0xFF);
                        valb -= 8;
                    }
                }
                point.state = decoded;
            }
        }
        
        return point;
    }
};

// Улучшенная конфигурация
struct RecoveryConfig {
    size_t maxRecoveryPoints = 10;
    std::chrono::seconds checkpointInterval = std::chrono::seconds(30);
    bool enableAutoRecovery = true;
    bool enableStateValidation = true;
    config::RecoveryPointConfig pointConfig;
    std::string logPath = "logs/recovery.log";
    size_t maxLogSize = 1024 * 1024; // 1MB по умолчанию
    size_t maxLogFiles = 2; // Значение по умолчанию
    
    // Валидация конфигурации
    bool validate() const {
        if (maxRecoveryPoints == 0) return false;
        if (checkpointInterval.count() <= 0) return false;
        if (pointConfig.maxSize == 0) return false;
        if (pointConfig.storagePath.empty()) return false;
        return true;
    }
};

// Основной класс с улучшенной структурой
class RecoveryManager {
public:
    explicit RecoveryManager(const RecoveryConfig& config = RecoveryConfig{}); // Конструктор
    RecoveryManager(const RecoveryManager&) = delete;
    RecoveryManager& operator=(const RecoveryManager&) = delete;
    RecoveryManager(RecoveryManager&&) noexcept;
<<<<<<< HEAD
=======
    RecoveryManager& operator=(RecoveryManager&&) noexcept;
    ~RecoveryManager(); // Деструктор
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
    bool initialize(); // Инициализация
    void shutdown();   // Завершение работы
    std::string createRecoveryPoint(); // Создать точку восстановления
    bool restoreFromPoint(const std::string& pointId); // Восстановить из точки
    void deleteRecoveryPoint(const std::string& pointId); // Удалить точку
    bool validateState(const std::vector<uint8_t>& state) const; // Валидация состояния
    void setConfiguration(const RecoveryConfig& config); // Установить конфиг
    RecoveryConfig getConfiguration() const; // Получить конфиг
    metrics::RecoveryMetrics getMetrics() const; // Метрики recovery
    std::chrono::steady_clock::time_point getLastCheckpointTime() const; // Время последнего чекпоинта
    bool isRecoveryInProgress() const; // Идёт ли восстановление
    void setStateCaptureCallback(std::function<std::vector<uint8_t>()> callback); // Callback захвата состояния
    void setStateRestoreCallback(std::function<bool(const std::vector<uint8_t>&)> callback); // Callback восстановления
    void setErrorCallback(std::function<void(const std::string&)> callback); // Callback ошибок
    void setLogLevel(spdlog::level::level_enum level); // Логирование
    void flushLogs(); // Сброс логов
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl; // Реализация
    std::shared_ptr<detail::RecoveryLogger> logger; // Логгер
    std::shared_ptr<detail::StateValidator> validator; // Валидатор
    std::shared_ptr<detail::CheckpointManager> checkpointManager; // Чекпоинты
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool; // Потоки
    mutable std::shared_mutex recoveryMutex; // Мьютекс
    std::atomic<bool> initialized; // Статус
    std::atomic<bool> recoveryInProgress; // Восстановление
    
    // Приватные методы
    void initializeLogger();
    void initializeValidator();
    void initializeCheckpointManager();
    void cleanupOldPoints();
    void validateRecoveryPoints();
    std::string generatePointId() const;
    bool saveRecoveryPoint(const RecoveryPoint& point);
    bool loadRecoveryPoint(const std::string& pointId, RecoveryPoint& point);
    void handleError(const std::string& error);
    
    // Вспомогательные методы
    std::string calculateChecksum(const std::vector<uint8_t>& data) const;
    bool compressState(std::vector<uint8_t>& data) const;
    bool decompressState(std::vector<uint8_t>& data) const;
    
    // Методы для работы с файловой системой
    bool ensureDirectoryExists(const std::string& path) const;
    std::string getStoragePath() const;
    
    // Методы для работы с метриками
    void updateMetrics(const metrics::RecoveryMetrics& newMetrics);
    void logMetrics() const;
};

} // namespace recovery
} // namespace core
} // namespace cloud 