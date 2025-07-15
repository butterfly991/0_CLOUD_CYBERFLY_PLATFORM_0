#include "core/kernel/advanced/ArchitecturalKernel.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/drivers/ARMDriver.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <atomic>

namespace cloud {
namespace core {
namespace kernel {

static std::atomic<bool> initialized{false};

// Добавить поле для кэшированных метрик
static metrics::PerformanceMetrics metrics_;

ArchitecturalKernel::ArchitecturalKernel() {
    // Инициализация компонентов
    platformOptimizer = core::cache::PlatformOptimizer();
    
    // Создание ARM драйвера для аппаратного ускорения
    hardwareAccelerator = std::make_unique<core::drivers::ARMDriver>();
    
    // Создание DynamicCache для архитектурных данных
    auto cacheConfig = platformOptimizer.getOptimalConfig();
    dynamicCache = std::make_unique<core::cache::DynamicCache<std::string, std::vector<uint8_t>>>(
        cacheConfig.maxEntries / 2, 7200 // 2 часа TTL для архитектурных данных
    );
    
    spdlog::info("ArchitecturalKernel: создан");
}

ArchitecturalKernel::~ArchitecturalKernel() {
    shutdown();
}

bool ArchitecturalKernel::initialize() {
    spdlog::info("ArchitecturalKernel: initialize() start");
    std::string storagePath = "./recovery_points/architectural";
    size_t cacheSize = 1024 * 1024; // 1MB по умолчанию
    // Проверка и дефолты
    if (storagePath.empty()) {
        storagePath = "./recovery_points/architectural";
        spdlog::warn("ArchitecturalKernel: storagePath был пуст, установлен по умолчанию: {}", storagePath);
    }
    if (cacheSize == 0) {
        cacheSize = 1024 * 1024;
        spdlog::warn("ArchitecturalKernel: cacheSize был 0, установлен по умолчанию: {}", cacheSize);
    }
    spdlog::info("ArchitecturalKernel: параметры: storagePath='{}', cacheSize={}", storagePath, cacheSize);
    try {
        spdlog::info("ArchitecturalKernel: инициализация ARM драйвера");
        if (!hardwareAccelerator->initialize()) {
            spdlog::warn("ArchitecturalKernel: ARM драйвер недоступен");
        } else {
            spdlog::info("ArchitecturalKernel: ARM драйвер инициализирован: {}", hardwareAccelerator->getPlatformInfo());
        }
        spdlog::info("ArchitecturalKernel: настройка DynamicCache");
        dynamicCache->setEvictionCallback([](const std::string& key, const std::vector<uint8_t>& data) {
            spdlog::debug("ArchitecturalKernel: архитектурные данные вытеснены: key={}, size={}", key, data.size());
        });
        dynamicCache->setAutoResize(true, 100, 2000);
        dynamicCache->setCleanupInterval(1800);
        initialized.store(true);
        spdlog::info("ArchitecturalKernel: инициализация завершена успешно");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка инициализации: {}", e.what());
        initialized.store(false);
        return false;
    } catch (...) {
        spdlog::error("ArchitecturalKernel: неизвестная ошибка инициализации");
        initialized.store(false);
        return false;
    }
}

void ArchitecturalKernel::shutdown() {
    spdlog::info("ArchitecturalKernel: shutdown() start");
    bool error = false;
    std::string errorMsg;
    try {
        // dynamicCache
        if (dynamicCache) {
            try { dynamicCache->clear(); dynamicCache.reset(); }
            catch (const std::exception& e) { spdlog::error("ArchitecturalKernel: dynamicCache exception: {}", e.what()); error = true; errorMsg = "dynamicCache: " + std::string(e.what()); }
        }
        // hardwareAccelerator
        if (hardwareAccelerator) {
            try { hardwareAccelerator->shutdown(); hardwareAccelerator.reset(); }
            catch (const std::exception& e) { spdlog::error("ArchitecturalKernel: hardwareAccelerator exception: {}", e.what()); error = true; errorMsg = "hardwareAccelerator: " + std::string(e.what()); }
        }
        // platformOptimizer
        try { /* platformOptimizer - не unique_ptr, просто reset если есть */ } catch (const std::exception& e) { spdlog::error("ArchitecturalKernel: platformOptimizer exception: {}", e.what()); error = true; errorMsg = "platformOptimizer: " + std::string(e.what()); }
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: shutdown() outer exception: {}", e.what());
        error = true;
        errorMsg = "outer: " + std::string(e.what());
    }
    initialized.store(false);
    spdlog::info("ArchitecturalKernel: shutdown() завершён (error: {})", error ? errorMsg : "none");
}

void ArchitecturalKernel::optimizeTopology() {
    try {
        spdlog::info("ArchitecturalKernel: оптимизация топологии");
        
        // Анализируем текущую архитектуру
        auto platformInfo = hardwareAccelerator ? hardwareAccelerator->getPlatformInfo() : "Unknown";
        spdlog::debug("ArchitecturalKernel: платформа: {}", platformInfo);
        
        // Определяем оптимальную конфигурацию кэша
        auto optimalConfig = platformOptimizer.getOptimalConfig();
        spdlog::debug("ArchitecturalKernel: оптимальная конфигурация кэша: {} записей", optimalConfig.maxEntries);
        
        // Применяем оптимизации
        if (dynamicCache) {
            dynamicCache->resize(optimalConfig.maxEntries);
            spdlog::debug("ArchitecturalKernel: размер кэша обновлен до {}", optimalConfig.maxEntries);
        }
        
        // Сохраняем информацию о топологии
        std::vector<uint8_t> topologyData;
        topologyData.reserve(1024);
        
        // Добавляем информацию о платформе
        for (char c : platformInfo) {
            topologyData.push_back(static_cast<uint8_t>(c));
        }
        
        // Добавляем информацию о возможностях
        if (hardwareAccelerator) {
            topologyData.push_back(hardwareAccelerator->isNeonSupported() ? 1 : 0);
            topologyData.push_back(hardwareAccelerator->isAMXSupported() ? 1 : 0);
            topologyData.push_back(hardwareAccelerator->isSVEAvailable() ? 1 : 0);
            topologyData.push_back(hardwareAccelerator->isNeuralEngineAvailable() ? 1 : 0);
        }
        
        // Сохраняем в кэш
        dynamicCache->put("topology_info", topologyData);
        
        spdlog::info("ArchitecturalKernel: оптимизация топологии завершена");
        
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка оптимизации топологии: {}", e.what());
    }
}

void ArchitecturalKernel::optimizePlacement() {
    try {
        spdlog::info("ArchitecturalKernel: оптимизация размещения");
        
        // Анализируем текущее размещение ресурсов
        if (dynamicCache) {
            size_t currentSize = dynamicCache->size();
            size_t allocatedSize = dynamicCache->allocatedSize();
            
            spdlog::debug("ArchitecturalKernel: текущее размещение - размер: {}, выделено: {}", 
                         currentSize, allocatedSize);
            
            // Оптимизируем размещение на основе использования
            if (currentSize < allocatedSize * 0.3) {
                // Слишком много свободного места
                size_t newSize = allocatedSize * 0.7;
                dynamicCache->resize(newSize);
                spdlog::debug("ArchitecturalKernel: уменьшен размер кэша до {}", newSize);
            } else if (currentSize > allocatedSize * 0.9) {
                // Мало свободного места
                size_t newSize = allocatedSize * 1.5;
                dynamicCache->resize(newSize);
                spdlog::debug("ArchitecturalKernel: увеличен размер кэша до {}", newSize);
            }
        }
        
        // Оптимизируем размещение на основе аппаратных возможностей
        if (hardwareAccelerator) {
            std::vector<uint8_t> placementData;
            
            // Создаем карту размещения ресурсов
            placementData.push_back(hardwareAccelerator->isNeonSupported() ? 1 : 0);
            placementData.push_back(hardwareAccelerator->isAMXSupported() ? 1 : 0);
            placementData.push_back(hardwareAccelerator->isSVEAvailable() ? 1 : 0);
            placementData.push_back(hardwareAccelerator->isNeuralEngineAvailable() ? 1 : 0);
            
            // Добавляем информацию о приоритетах размещения
            placementData.push_back(0x01); // CPU приоритет
            placementData.push_back(0x02); // Memory приоритет
            placementData.push_back(0x03); // Cache приоритет
            
            // Сохраняем в кэш
            dynamicCache->put("placement_info", placementData);
        }
        
        spdlog::info("ArchitecturalKernel: оптимизация размещения завершена");
        
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка оптимизации размещения: {}", e.what());
    }
}

void ArchitecturalKernel::updateMetrics() {
    try {
        metrics_ = metrics::PerformanceMetrics{};
        if (dynamicCache) {
            metrics_.memory_usage = static_cast<double>(dynamicCache->size()) / 1000.0;
        }
        if (hardwareAccelerator) {
            metrics_.efficiencyScore = hardwareAccelerator->isNeonSupported() ? 0.85 : 0.6;
        } else {
            metrics_.efficiencyScore = 0.5;
        }
        metrics_.timestamp = std::chrono::steady_clock::now();
        spdlog::trace("ArchitecturalKernel: метрики обновлены");
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка обновления метрик: {}", e.what());
    }
}

bool ArchitecturalKernel::isRunning() const {
    return initialized.load();
}

metrics::PerformanceMetrics ArchitecturalKernel::getMetrics() const {
    return metrics_;
}

void ArchitecturalKernel::setResourceLimit(const std::string& resource, double limit) {
    try {
        if (resource == "cache" && dynamicCache) {
            dynamicCache->resize(static_cast<size_t>(limit));
            spdlog::info("ArchitecturalKernel: установлен лимит кэша {}", limit);
        } else {
            spdlog::warn("ArchitecturalKernel: неизвестный ресурс '{}'", resource);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка установки лимита ресурса: {}", e.what());
    }
}

double ArchitecturalKernel::getResourceUsage(const std::string& resource) const {
    try {
        if (resource == "cache" && dynamicCache) {
            return static_cast<double>(dynamicCache->size());
        } else {
            spdlog::warn("ArchitecturalKernel: неизвестный ресурс '{}'", resource);
            return 0.0;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка получения использования ресурса: {}", e.what());
        return 0.0;
    }
}

KernelType ArchitecturalKernel::getType() const {
    return KernelType::ARCHITECTURAL;
}

std::string ArchitecturalKernel::getId() const {
    return "architectural_kernel";
}

void ArchitecturalKernel::pause() {
    spdlog::info("ArchitecturalKernel: приостановлен");
}

void ArchitecturalKernel::resume() {
    spdlog::info("ArchitecturalKernel: возобновлен");
}

void ArchitecturalKernel::reset() {
    try {
    if (dynamicCache) {
            dynamicCache->clear();
        }
        
        spdlog::info("ArchitecturalKernel: сброшен");
        
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка сброса: {}", e.what());
    }
}

std::vector<std::string> ArchitecturalKernel::getSupportedFeatures() const {
    std::vector<std::string> features = {
        "topology_optimization",
        "placement_optimization",
        "hardware_acceleration",
        "cache_optimization",
        "platform_analysis"
    };
    
    if (hardwareAccelerator) {
        if (hardwareAccelerator->isNeonSupported()) {
            features.push_back("neon_support");
        }
        if (hardwareAccelerator->isAMXSupported()) {
            features.push_back("amx_support");
        }
        if (hardwareAccelerator->isSVEAvailable()) {
            features.push_back("sve_support");
        }
        if (hardwareAccelerator->isNeuralEngineAvailable()) {
            features.push_back("neural_engine_support");
        }
    }
    
    return features;
}

void ArchitecturalKernel::scheduleTask(std::function<void()> task, int priority) {
    // Архитектурное ядро не использует ThreadPool, выполняем задачу синхронно
    try {
        task();
    } catch (const std::exception& e) {
        spdlog::error("ArchitecturalKernel: ошибка выполнения задачи: {}", e.what());
    }
}

} // namespace kernel
} // namespace core
} // namespace cloud
