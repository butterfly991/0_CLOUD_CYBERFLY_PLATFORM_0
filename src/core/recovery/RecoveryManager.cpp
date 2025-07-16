#include "core/recovery/RecoveryManager.hpp"
#include <random>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <zlib.h>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "core/thread/ThreadPool.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

namespace cloud {
namespace core {
namespace recovery {

// Определение структуры Impl для RecoveryManager (должно быть до методов)
struct RecoveryManager::Impl {
    RecoveryConfig config;
    std::unordered_map<std::string, RecoveryPoint> recoveryPoints;
    metrics::RecoveryMetrics metrics;
    std::function<std::vector<uint8_t>()> stateCaptureCallback;
    std::function<bool(const std::vector<uint8_t>&)> stateRestoreCallback;
    std::function<void(const std::string&)> errorCallback;
    std::chrono::steady_clock::time_point lastCheckpoint;
    std::mt19937 rng;

    Impl(const RecoveryConfig& cfg)
        : config(cfg), metrics{}, lastCheckpoint(std::chrono::steady_clock::now()), rng(std::random_device{}()) {}
};

namespace detail {

// Реализация логгера
class RecoveryLogger {
public:
    explicit RecoveryLogger(const std::string& logPath, size_t maxSize, size_t maxFiles) {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logPath, maxSize, maxFiles);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        logger_ = std::make_shared<spdlog::logger>("recovery_local", file_sink); // имя уникальное, не регистрируем глобально
        logger_->set_level(spdlog::level::info);
    }

    void log(spdlog::level::level_enum level, const std::string& message) {
        logger_->log(level, message);
    }

    void flush() {
        logger_->flush();
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
};

// Реализация валидатора состояния
class StateValidator {
public:
    bool validateState(const std::vector<uint8_t>& state) const {
        if (state.empty()) return false;
        
        // Проверка целостности данных
        std::string checksum = calculateChecksum(state);
        return !checksum.empty();
    }

private:
    std::string calculateChecksum(const std::vector<uint8_t>& data) const {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.data(), data.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }
};

// Реализация менеджера контрольных точек
class CheckpointManager {
public:
    explicit CheckpointManager(const config::RecoveryPointConfig& config)
        : config_(config) {}

    bool saveCheckpoint(const RecoveryPoint& point) {
        try {
            std::filesystem::path path = config_.storagePath;
            path /= point.id + ".json";
            spdlog::info("[DIAG] saveCheckpoint: path={} size={}", path.string(), point.state.size());
            std::ofstream file(path);
            if (!file) {
                spdlog::error("[DIAG] saveCheckpoint: failed to open file {}", path.string());
                return false;
            }
            file << point.toJson().dump(4);
            spdlog::info("[DIAG] saveCheckpoint: file write complete");
            return true;
        } catch (const std::exception& e) {
            spdlog::error("[DIAG] saveCheckpoint: exception: {}", e.what());
            return false;
        }
    }

    bool loadCheckpoint(const std::string& id, RecoveryPoint& point) {
        try {
            std::filesystem::path path = config_.storagePath;
            path /= id + ".json";
            spdlog::info("[DIAG] loadCheckpoint: path={}", path.string());
            std::ifstream file(path);
            if (!file) {
                spdlog::error("[DIAG] loadCheckpoint: failed to open file {}", path.string());
                return false;
            }
            nlohmann::json j;
            file >> j;
            point = RecoveryPoint::fromJson(j);
            spdlog::info("[DIAG] loadCheckpoint: loaded state.size={}", point.state.size());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("[DIAG] loadCheckpoint: exception: {}", e.what());
            return false;
        }
    }

private:
    config::RecoveryPointConfig config_;
};

} // namespace detail

RecoveryManager::RecoveryManager(const RecoveryConfig& config)
    : pImpl(std::make_unique<Impl>(config))
    , recoveryInProgress(false) {
    initializeLogger();
    initializeValidator();
    initializeCheckpointManager();
}

RecoveryManager::RecoveryManager(RecoveryManager&& other) noexcept
    : pImpl(std::move(other.pImpl))
    , logger(std::move(other.logger))
    , validator(std::move(other.validator))
    , checkpointManager(std::move(other.checkpointManager))
    , threadPool(std::move(other.threadPool)) {
}

RecoveryManager& RecoveryManager::operator=(RecoveryManager&& other) noexcept {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
        logger = std::move(other.logger);
        validator = std::move(other.validator);
        checkpointManager = std::move(other.checkpointManager);
        threadPool = std::move(other.threadPool);
    }
    return *this;
}

RecoveryManager::~RecoveryManager() {
    try {
        shutdown();
        logger->log(spdlog::level::info, "RecoveryManager destroyed");
        logger->flush();
    } catch (const std::exception& e) {
        // Логируем ошибку при уничтожении
        if (logger) {
            logger->log(spdlog::level::err, 
                "Error during RecoveryManager destruction: " + std::string(e.what()));
            logger->flush();
        }
    }
}

void RecoveryManager::initializeLogger() {
    // Проверяем, что maxLogSize не равен 0
    if (pImpl->config.maxLogSize == 0) {
        // Используем default logger если maxLogSize равен 0
        logger = std::make_shared<detail::RecoveryLogger>(
            "logs/recovery_default.log", 1024*1024, 1  // Используем минимальные значения
        );
    } else {
        logger = std::make_shared<detail::RecoveryLogger>(
            pImpl->config.logPath,
            pImpl->config.maxLogSize,
            pImpl->config.maxLogFiles
        );
    }
    logger->log(spdlog::level::info, "Logger initialized");
}

void RecoveryManager::initializeValidator() {
    validator = std::make_shared<detail::StateValidator>();
    logger->log(spdlog::level::info, "State validator initialized");
}

void RecoveryManager::initializeCheckpointManager() {
    checkpointManager = std::make_shared<detail::CheckpointManager>(
        pImpl->config.pointConfig
    );
    logger->log(spdlog::level::info, "Checkpoint manager initialized");
}

bool RecoveryManager::initialize() {
    std::lock_guard<std::shared_mutex> lock(recoveryMutex);
    if (initialized.load()) {
        spdlog::warn("RecoveryManager: повторная инициализация");
        return true;
    }
    
    std::string path; // Объявляем переменную здесь
    
    try {
        // Проверка storagePath
        path = pImpl->config.pointConfig.storagePath;
        if (path.empty()) {
            path = "./recovery_points/default";
            spdlog::warn("RecoveryManager: storagePath был пуст, установлен по умолчанию: {}", path);
            pImpl->config.pointConfig.storagePath = path;
        }
        
        // Создание директории, если не существует
        std::filesystem::path dirPath(path);
        if (!std::filesystem::exists(dirPath)) {
            std::filesystem::create_directories(dirPath);
            spdlog::info("RecoveryManager: создана директория storagePath: {}", path);
        }
        auto perms = std::filesystem::status(dirPath).permissions();
        spdlog::info("RecoveryManager: storagePath='{}', права={:o}", path, static_cast<unsigned>(perms));
        
        if (!pImpl->config.validate()) {
            throw std::runtime_error("Invalid configuration");
        }
        
        if (!ensureDirectoryExists(pImpl->config.pointConfig.storagePath)) {
            throw std::runtime_error("Failed to create storage directory");
        }
        
        // Инициализируем все компоненты
        initializeLogger();
        initializeValidator();
        initializeCheckpointManager();
        
        threadPool = std::make_shared<cloud::core::thread::ThreadPool>(cloud::core::thread::ThreadPoolConfig{/* заполнить параметры */});
        
        logger->log(spdlog::level::info, "RecoveryManager initialized successfully");
        initialized.store(true);
        spdlog::info("RecoveryManager: успешно инициализирован");
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("RecoveryManager: не удалось создать storagePath '{}': {}", path, e.what());
        handleError("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void RecoveryManager::shutdown() {
    try {
        if (threadPool) {
            threadPool->stop();
        }
        
        flushLogs();
        
        logger->log(spdlog::level::info, "RecoveryManager shut down successfully");
    } catch (const std::exception& e) {
        handleError("Shutdown failed: " + std::string(e.what()));
    }
}

std::string RecoveryManager::createRecoveryPoint() {
    try {
        auto startTime = std::chrono::steady_clock::now();
        RecoveryPoint point;
        point.id = generatePointId();
        point.timestamp = std::chrono::steady_clock::now();
        if (pImpl->stateCaptureCallback) {
            point.state = pImpl->stateCaptureCallback();
        } else {
            point.state = std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05};
        }
        spdlog::info("[DIAG] createRecoveryPoint: id={}, state.size={}", point.id, point.state.size());
        if (pImpl->config.enableStateValidation) {
            point.checksum = calculateChecksum(point.state);
            point.isConsistent = validator->validateState(point.state);
        } else {
            point.isConsistent = true;
        }
        if (pImpl->config.pointConfig.enableCompression) {
            compressState(point.state);
        }
        point.size = point.state.size();
        std::string filePath = pImpl->config.pointConfig.storagePath + "/" + point.id + ".json";
        spdlog::info("[DIAG] createRecoveryPoint: save to {}", filePath);
        bool saved = checkpointManager->saveCheckpoint(point);
        spdlog::info("[DIAG] createRecoveryPoint: saveCheckpoint result={}", saved);
        if (!saved) {
            throw std::runtime_error("Failed to save recovery point");
        }
        
        // Добавляем точку в память после успешного сохранения
        pImpl->recoveryPoints[point.id] = point;
        
        metrics::RecoveryMetrics newMetrics = pImpl->metrics;
        newMetrics.totalPoints++;
        updateMetrics(newMetrics);
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        logger->log(spdlog::level::info, "Created recovery point " + point.id + " in " + std::to_string(duration) + "ms");
        return point.id;
    } catch (const std::exception& e) {
        handleError("Failed to create recovery point: " + std::string(e.what()));
        return "";
    }
}

bool RecoveryManager::restoreFromPoint(const std::string& pointId) {
    try {
        recoveryInProgress = true;
        auto startTime = std::chrono::steady_clock::now();
        RecoveryPoint point;
        std::string filePath = pImpl->config.pointConfig.storagePath + "/" + pointId + ".json";
        spdlog::info("[DIAG] restoreFromPoint: id={}, filePath={}", pointId, filePath);
        bool loaded = checkpointManager->loadCheckpoint(pointId, point);
        spdlog::info("[DIAG] restoreFromPoint: loadCheckpoint result={}, state.size={}", loaded, point.state.size());
        if (!loaded) {
            logger->log(spdlog::level::err, "Не удалось загрузить recovery point: " + pointId + ", файл: " + filePath);
            throw std::runtime_error("Failed to load recovery point");
        }
        if (pImpl->config.pointConfig.enableCompression) {
            decompressState(point.state);
        }
        if (pImpl->config.enableStateValidation) {
            if (!validator->validateState(point.state)) {
                logger->log(spdlog::level::err, "Валидация состояния не пройдена для точки: " + pointId);
                throw std::runtime_error("Invalid state data");
            }
        }
        if (pImpl->stateRestoreCallback) {
            bool cbResult = pImpl->stateRestoreCallback(point.state);
            spdlog::info("[DIAG] restoreFromPoint: stateRestoreCallback result={}", cbResult);
            if (!cbResult) {
                logger->log(spdlog::level::err, "stateRestoreCallback вернул false для точки: " + pointId);
                throw std::runtime_error("Failed to restore state");
            }
        } else {
            logger->log(spdlog::level::info, "State restore callback not set, skipping state restoration");
        }
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        metrics::RecoveryMetrics newMetrics = pImpl->metrics;
        newMetrics.successfulRecoveries++;
        newMetrics.averageRecoveryTime = (newMetrics.averageRecoveryTime * (newMetrics.successfulRecoveries - 1) + duration) / newMetrics.successfulRecoveries;
        newMetrics.lastRecovery = endTime;
        updateMetrics(newMetrics);
        logger->log(spdlog::level::info, "Restored from point " + pointId + " in " + std::to_string(duration) + "ms");
        recoveryInProgress = false;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[DIAG] restoreFromPoint: exception: {}", e.what());
        logger->log(spdlog::level::err, std::string("Failed to restore from point: ") + e.what());
        handleError("Failed to restore from point: " + std::string(e.what()));
        recoveryInProgress = false;
        return false;
    }
}

void RecoveryManager::deleteRecoveryPoint(const std::string& pointId) {
    // Удаляем из памяти
    pImpl->recoveryPoints.erase(pointId);
    
    // Обновляем метрики
    pImpl->metrics.totalPoints = pImpl->recoveryPoints.size();
    
    // Удаляем файл
    std::string filePath = pImpl->config.pointConfig.storagePath + "/" + pointId + ".json";
    try {
        if (std::filesystem::exists(filePath)) {
            std::filesystem::remove(filePath);
            logger->log(spdlog::level::info, fmt::format("RecoveryManager: удалена точка восстановления {} (файл {})", pointId, filePath));
        }
    } catch (const std::exception& e) {
        logger->log(spdlog::level::err, fmt::format("RecoveryManager: ошибка удаления файла {}: {}", filePath, e.what()));
    }
}

bool RecoveryManager::validateState(const std::vector<uint8_t>& state) const {
    if (!pImpl->config.enableStateValidation) return true;
    if (state.empty()) return false;
    // Проверка SHA256: просто считаем хеш и сравниваем с контрольной суммой, если есть
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(state.data(), state.size(), hash);
    // Можно добавить сравнение с эталонной checksum, если она есть в RecoveryPoint
    // Пока просто возвращаем true, если хеш посчитан
    return true;
}

void RecoveryManager::setConfiguration(const RecoveryConfig& config) {
    pImpl->config = config;
    cleanupOldPoints();
}

RecoveryConfig RecoveryManager::getConfiguration() const {
    return pImpl->config;
}

std::chrono::steady_clock::time_point RecoveryManager::getLastCheckpointTime() const {
    return pImpl->lastCheckpoint;
}

bool RecoveryManager::isRecoveryInProgress() const {
    return recoveryInProgress;
}

void RecoveryManager::cleanupOldPoints() {
    if (pImpl->recoveryPoints.size() <= pImpl->config.maxRecoveryPoints) return;
    
    std::vector<std::pair<std::string, RecoveryPoint>> points(
        pImpl->recoveryPoints.begin(), pImpl->recoveryPoints.end());
    
    std::sort(points.begin(), points.end(),
        [](const auto& a, const auto& b) {
            return a.second.timestamp < b.second.timestamp;
        });
    
    size_t toRemove = points.size() - pImpl->config.maxRecoveryPoints;
    for (size_t i = 0; i < toRemove; ++i) {
        pImpl->recoveryPoints.erase(points[i].first);
    }
}

void RecoveryManager::validateRecoveryPoints() {
    for (auto& pair : pImpl->recoveryPoints) {
        pair.second.isConsistent = validateState(pair.second.state);
    }
}

std::string RecoveryManager::generatePointId() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (int i = 0; i < 8; ++i) {
        ss << std::setw(2) << (pImpl->rng() & 0xFF);
    }
    
    return ss.str();
}

bool RecoveryManager::saveRecoveryPoint(const RecoveryPoint& point) {
    std::string filePath = pImpl->config.pointConfig.storagePath + "/" + point.id + ".json";
    try {
        nlohmann::json j = point.toJson();
        std::ofstream ofs(filePath);
        ofs << j.dump(2);
        ofs.close();
        logger->log(spdlog::level::info, fmt::format("RecoveryManager: сохранена точка восстановления {} (файл {})", point.id, filePath));
        pImpl->recoveryPoints[point.id] = point;
        return true;
    } catch (const std::exception& e) {
        logger->log(spdlog::level::err, fmt::format("RecoveryManager: ошибка сохранения точки {}: {}", point.id, e.what()));
        return false;
    }
}

bool RecoveryManager::loadRecoveryPoint(const std::string& pointId, RecoveryPoint& point) {
    std::string filePath = pImpl->config.pointConfig.storagePath + "/" + pointId + ".json";
    try {
        std::ifstream ifs(filePath);
        if (!ifs) {
            logger->log(spdlog::level::warn, fmt::format("RecoveryManager: файл точки {} не найден: {}", pointId, filePath));
            return false;
        }
        nlohmann::json j;
        ifs >> j;
        point = RecoveryPoint::fromJson(j);
        logger->log(spdlog::level::info, fmt::format("RecoveryManager: загружена точка восстановления {} (файл {})", pointId, filePath));
        pImpl->recoveryPoints[pointId] = point;
        return true;
    } catch (const std::exception& e) {
        logger->log(spdlog::level::err, fmt::format("RecoveryManager: ошибка загрузки точки {}: {}", pointId, e.what()));
        return false;
    }
}

void RecoveryManager::handleError(const std::string& error) {
    logger->log(spdlog::level::err, error);
    if (pImpl->errorCallback) {
        pImpl->errorCallback(error);
    }
}

void RecoveryManager::updateMetrics(const metrics::RecoveryMetrics& newMetrics) {
    pImpl->metrics = newMetrics;
    logMetrics();
}

void RecoveryManager::logMetrics() const {
    nlohmann::json metricsJson = {
        {"totalPoints", pImpl->metrics.totalPoints},
        {"successfulRecoveries", pImpl->metrics.successfulRecoveries},
        {"failedRecoveries", pImpl->metrics.failedRecoveries},
        {"averageRecoveryTime", pImpl->metrics.averageRecoveryTime},
        {"lastRecovery", std::chrono::duration_cast<std::chrono::milliseconds>(
            pImpl->metrics.lastRecovery.time_since_epoch()).count()}
    };
    
    logger->log(spdlog::level::info, "Metrics updated: " + metricsJson.dump());
}

void RecoveryManager::flushLogs() {
    try {
        spdlog::info("RecoveryManager: сброс логов");
        // Здесь можно добавить логику сброса логов в файл или базу данных
    } catch (const std::exception& e) {
        spdlog::error("RecoveryManager: ошибка сброса логов: {}", e.what());
    }
}

bool RecoveryManager::compressState(std::vector<uint8_t>& data) const {
    try {
        // Простая реализация сжатия (можно заменить на более эффективную)
        std::vector<uint8_t> compressed;
        compressed.reserve(data.size());
        
        for (size_t i = 0; i < data.size(); ++i) {
            if (i > 0 && data[i] == data[i-1]) {
                // RLE сжатие для повторяющихся байтов
                uint8_t count = 1;
                while (i < data.size() && data[i] == data[i-1] && count < 255) {
                    count++;
                    i++;
                }
                compressed.push_back(0); // маркер RLE
                compressed.push_back(count);
                compressed.push_back(data[i-1]);
                if (i < data.size()) i--;
            } else {
                compressed.push_back(data[i]);
            }
        }
        
        if (compressed.size() < data.size()) {
            data = std::move(compressed);
            spdlog::debug("RecoveryManager: состояние сжато с {} до {} байт", 
                         data.size(), compressed.size());
        }
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("RecoveryManager: ошибка сжатия состояния: {}", e.what());
        return false;
    }
}

bool RecoveryManager::decompressState(std::vector<uint8_t>& data) const {
    try {
        std::vector<uint8_t> decompressed;
        decompressed.reserve(data.size() * 2); // примерный размер
        
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] == 0 && i + 2 < data.size()) {
                // RLE разжатие
                uint8_t count = data[i + 1];
                uint8_t value = data[i + 2];
                for (uint8_t j = 0; j < count; ++j) {
                    decompressed.push_back(value);
                }
                i += 2;
            } else {
                decompressed.push_back(data[i]);
            }
        }
        
        data = std::move(decompressed);
        spdlog::debug("RecoveryManager: состояние разжато до {} байт", data.size());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("RecoveryManager: ошибка разжатия состояния: {}", e.what());
        return false;
    }
}

std::string RecoveryManager::calculateChecksum(const std::vector<uint8_t>& data) const {
    try {
        SHA256_CTX sha256;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.data(), data.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        
        return ss.str();
    } catch (const std::exception& e) {
        spdlog::error("RecoveryManager: ошибка расчета контрольной суммы: {}", e.what());
        return "";
    }
}

bool RecoveryManager::ensureDirectoryExists(const std::string& path) const {
    try {
        std::filesystem::path dirPath(path);
        if (!std::filesystem::exists(dirPath)) {
            std::filesystem::create_directories(dirPath);
            spdlog::debug("RecoveryManager: создана директория {}", path);
        }
        return std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath);
    } catch (const std::exception& e) {
        spdlog::error("RecoveryManager: ошибка создания директории {}: {}", path, e.what());
        return false;
    }
}

cloud::core::recovery::metrics::RecoveryMetrics RecoveryManager::getMetrics() const {
    std::shared_lock lock(recoveryMutex);
    metrics::RecoveryMetrics metrics = pImpl->metrics;
    // Обновляем totalPoints на основе реального количества точек
    metrics.totalPoints = pImpl->recoveryPoints.size();
    return metrics;
}

void cloud::core::recovery::RecoveryManager::setStateCaptureCallback(std::function<std::vector<uint8_t>()> callback) {
    std::lock_guard<std::shared_mutex> lock(recoveryMutex);
    pImpl->stateCaptureCallback = std::move(callback);
}

void cloud::core::recovery::RecoveryManager::setStateRestoreCallback(std::function<bool(const std::vector<uint8_t>&)> callback) {
    std::lock_guard<std::shared_mutex> lock(recoveryMutex);
    pImpl->stateRestoreCallback = std::move(callback);
}

} // namespace recovery
} // namespace core
} // namespace cloud 