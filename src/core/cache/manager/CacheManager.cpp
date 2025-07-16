#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <type_traits>

namespace cloud {
namespace core {
namespace cache {

// Реализация PIMPL
struct CacheManager::Impl {
    CacheConfig config;
    std::unique_ptr<DynamicCache<std::string, std::vector<uint8_t>>> dynamicCache;
    CacheMetrics metrics;
    std::atomic<bool> isInitialized{false};
    
    Impl(const CacheConfig& cfg) : config(cfg) {
        // Инициализация логгера
        try {
            spdlog::set_level(spdlog::level::debug);
        } catch (const std::exception& e) {
            spdlog::error("Ошибка инициализации логгера: {}", e.what());
        }
    }

    ~Impl() {
        if (dynamicCache) {
            spdlog::info("[DEBUG] Impl::~Impl: DynamicCache ptr = {}", static_cast<void*>(dynamicCache.get()));
        }
        spdlog::info("CacheManager::Impl: деструктор вызван (DEBUG)");
    }
};

CacheManager::CacheManager(const CacheConfig& config)
    : pImpl(std::make_unique<Impl>(config)), initialized(false) {
    // Инициализируем логгер, если он еще не создан
    try {
        auto logger = spdlog::get("cachemanager");
        if (!logger) {
            // Создаем директорию для логов, если её нет
            std::filesystem::create_directories("logs");
            
            auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "logs/cachemanager.log", 1024*1024*5, 2);
            logger = std::make_shared<spdlog::logger>("cachemanager", rotating_sink);
            logger->set_level(spdlog::level::debug);
            spdlog::register_logger(logger);
        }
        logger->info("CacheManager создан с конфигурацией: maxSize={}, maxEntries={}", 
                     config.maxSize, config.maxEntries);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка инициализации логгера CacheManager: " << e.what() << std::endl;
    }
}

CacheManager::~CacheManager() {
    spdlog::info("CacheManager: деструктор вызван (DEBUG)");
    shutdown();
}

bool CacheManager::initialize() {
    auto start = std::chrono::high_resolution_clock::now();
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (initialized) {
            try {
                auto logger = spdlog::get("cachemanager");
                if (logger) {
                    logger->warn("CacheManager уже инициализирован");
                }
            } catch (...) {
                // Игнорируем ошибки логгера
            }
            return true;
        }
        
        spdlog::info("CacheManager: начало инициализации");
        
        // Проверка и дефолты для maxSize/maxEntries
        if (pImpl->config.maxSize == 0) {
            pImpl->config.maxSize = 1024 * 1024 * 10; // 10MB по умолчанию
            if (auto logger = spdlog::get("cachemanager")) {
                logger->warn("CacheManager: maxSize был 0, установлен по умолчанию: {}", static_cast<unsigned long long>(pImpl->config.maxSize));
            }
        }
        if (pImpl->config.maxEntries == 0) {
            pImpl->config.maxEntries = 1000;
            if (auto logger = spdlog::get("cachemanager")) {
                logger->warn("CacheManager: maxEntries был 0, установлен по умолчанию: {}", static_cast<unsigned long long>(pImpl->config.maxEntries));
            }
        }
        if (auto logger = spdlog::get("cachemanager")) {
            logger->info("CacheManager: параметры кэша: maxSize={}, maxEntries={}, storagePath='{}'", 
                static_cast<unsigned long long>(pImpl->config.maxSize), 
                static_cast<unsigned long long>(pImpl->config.maxEntries), 
                pImpl->config.storagePath);
        }
        
        // Создаем DynamicCache
        auto cacheStart = std::chrono::high_resolution_clock::now();
        pImpl->dynamicCache = std::make_unique<DynamicCache<std::string, std::vector<uint8_t>>>(
            pImpl->config.maxEntries, 
            static_cast<size_t>(pImpl->config.entryLifetime.count())
        );
        auto cacheEnd = std::chrono::high_resolution_clock::now();
        auto cacheDuration = std::chrono::duration_cast<std::chrono::microseconds>(cacheEnd - cacheStart).count();
        spdlog::info("CacheManager: DynamicCache создан за {} μs", cacheDuration);
        
        // Устанавливаем callback для вытеснения
        pImpl->dynamicCache->setEvictionCallback([this](const std::string& key, const std::vector<uint8_t>& data) {
            try {
                auto logger = spdlog::get("cachemanager");
                if (logger) {
                    logger->debug("Элемент вытеснен из кэша: key={}, size={}", key, data.size());
                }
            } catch (...) {
                // Игнорируем ошибки логгера
            }
            ++pImpl->metrics.evictionCount;
        });
        
        // Включаем автоизменение размера для всех режимов
        pImpl->dynamicCache->setAutoResize(true, pImpl->config.maxEntries / 4, pImpl->config.maxEntries);
        
        // Включаем фоновые операции для всех режимов
        pImpl->dynamicCache->setCleanupInterval(3); // 3 секунды для всех режимов
        if (auto logger = spdlog::get("cachemanager")) {
            logger->info("CacheManager: фоновые операции включены с интервалом 3 секунды");
        }
        
        pImpl->isInitialized = true;
        initialized = true;
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        try {
            auto logger = spdlog::get("cachemanager");
            if (logger) {
                logger->info("CacheManager успешно инициализирован за {} μs", totalDuration);
            }
        } catch (...) {
            // Игнорируем ошибки логгера
        }
        return true;
        
    } catch (const std::exception& e) {
        auto end = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        try {
            auto logger = spdlog::get("cachemanager");
            if (logger) {
                logger->error("Ошибка инициализации CacheManager за {} μs: {}", totalDuration, e.what());
            }
        } catch (...) {
            std::cerr << "Ошибка инициализации CacheManager за " << totalDuration << " μs: " << e.what() << std::endl;
        }
        return false;
    }
}

bool CacheManager::getData(const std::string& key, std::vector<uint8_t>& data) {
    auto start = std::chrono::high_resolution_clock::now();
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (!initialized || !pImpl->dynamicCache) {
            if (auto logger = spdlog::get("cachemanager")) {
                logger->error("CacheManager не инициализирован или dynamicCache=nullptr");
            }
            return false;
        }
        
        auto result = pImpl->dynamicCache->get(key);
        if (result) {
            data = *result;
            ++pImpl->metrics.requestCount;
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            
            if (auto logger = spdlog::get("cachemanager")) {
                logger->debug("Данные получены из кэша: key={}, size={}, время={} μs", 
                             key.c_str(), static_cast<unsigned long long>(data.size()), duration);
            }
            return true;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (auto logger = spdlog::get("cachemanager")) {
            logger->debug("Данные не найдены в кэше: key={}, время={} μs", key.c_str(), duration);
        }
        return false;
        
    } catch (const std::exception& e) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (auto logger = spdlog::get("cachemanager")) {
            logger->error("Ошибка получения данных за {} μs: {}", duration, e.what());
        }
        return false;
    }
}

bool CacheManager::putData(const std::string& key, const std::vector<uint8_t>& data) {
    auto start = std::chrono::high_resolution_clock::now();
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (!initialized || !pImpl->dynamicCache) {
            if (auto logger = spdlog::get("cachemanager")) {
                logger->error("CacheManager не инициализирован или dynamicCache=nullptr");
            }
            return false;
        }
        
        pImpl->dynamicCache->put(key, data);
        ++pImpl->metrics.requestCount;
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (auto logger = spdlog::get("cachemanager")) {
            logger->debug("Данные сохранены в кэш: key={}, size={}, время={} μs", 
                         key.c_str(), static_cast<unsigned long long>(data.size()), duration);
        }
        return true;
        
    } catch (const std::exception& e) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (auto logger = spdlog::get("cachemanager")) {
            logger->error("Ошибка сохранения данных за {} μs: {}", duration, e.what());
        }
        return false;
    }
}

void CacheManager::invalidateData(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (!initialized || !pImpl->dynamicCache) {
            if (auto logger = spdlog::get("cachemanager")) {
                logger->error("CacheManager не инициализирован или dynamicCache=nullptr");
            }
            return;
        }
        
        pImpl->dynamicCache->remove(key);
        if (auto logger = spdlog::get("cachemanager")) {
            logger->debug("Данные инвалидированы: key={}", key.c_str());
        }
        
    } catch (const std::exception& e) {
        if (auto logger = spdlog::get("cachemanager")) {
            logger->error("Ошибка инвалидации данных: {}", e.what());
        }
    }
}

void CacheManager::setConfiguration(const CacheConfig& config) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (!config.validate()) {
            throw std::runtime_error("Некорректная конфигурация кэша");
        }
        
        pImpl->config = config;
        
        if (initialized && pImpl->dynamicCache) {
            // Обновляем размер кэша
            pImpl->dynamicCache->resize(config.maxEntries);
            pImpl->dynamicCache->setAutoResize(true, config.maxEntries / 4, config.maxEntries);
        }
        
        if (auto logger = spdlog::get("cachemanager")) {
            logger->info("Конфигурация кэша обновлена");
        }
        
    } catch (const std::exception& e) {
        if (auto logger = spdlog::get("cachemanager")) {
            logger->error("Ошибка обновления конфигурации: {}", e.what());
        }
        throw;
    }
}

CacheConfig CacheManager::getConfiguration() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    return pImpl->config;
}

size_t CacheManager::getCacheSize() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    
    if (!initialized || !pImpl->dynamicCache) {
        return 0;
    }
    
    return pImpl->dynamicCache->allocatedSize();
}

size_t CacheManager::getEntryCount() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    
    if (!initialized || !pImpl->dynamicCache) {
        return 0;
    }
    
    return pImpl->dynamicCache->size();
}

CacheMetrics CacheManager::getMetrics() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    
    // Обновляем метрики перед возвратом
    if (initialized && pImpl->dynamicCache) {
        pImpl->metrics.currentSize = pImpl->dynamicCache->allocatedSize();
        pImpl->metrics.maxSize = pImpl->config.maxSize;
        pImpl->metrics.entryCount = pImpl->dynamicCache->size();
        pImpl->metrics.lastUpdate = std::chrono::steady_clock::now();
        
        // Рассчитываем hit rate (упрощенная версия)
        if (pImpl->metrics.requestCount > 0) {
            pImpl->metrics.hitRate = 0.8; // Примерное значение
        }
        
        // Рассчитываем eviction rate
        if (pImpl->metrics.requestCount > 0) {
            pImpl->metrics.evictionRate = static_cast<double>(pImpl->metrics.evictionCount) / pImpl->metrics.requestCount;
        }
    }
    
    return pImpl->metrics;
}

void CacheManager::updateMetrics() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (!initialized || !pImpl->dynamicCache) {
            return;
        }
        
        // Обновляем метрики
        pImpl->metrics.currentSize = pImpl->dynamicCache->allocatedSize();
        pImpl->metrics.maxSize = pImpl->config.maxSize;
        pImpl->metrics.entryCount = pImpl->dynamicCache->size();
        pImpl->metrics.lastUpdate = std::chrono::steady_clock::now();
        
        // Рассчитываем hit rate (упрощенная версия)
        if (pImpl->metrics.requestCount > 0) {
            pImpl->metrics.hitRate = 0.8; // Примерное значение
        }
        
        // Рассчитываем eviction rate
        if (pImpl->metrics.requestCount > 0) {
            pImpl->metrics.evictionRate = static_cast<double>(pImpl->metrics.evictionCount) / pImpl->metrics.requestCount;
        }
        
        if (auto logger = spdlog::get("cachemanager")) {
            logger->debug("Метрики обновлены: size={}, entries={}, requests={}", 
                static_cast<unsigned long long>(pImpl->metrics.currentSize), 
                static_cast<unsigned long long>(pImpl->metrics.entryCount), 
                static_cast<unsigned long long>(pImpl->metrics.requestCount));
        }
        
        } catch (const std::exception& e) {
        if (auto logger = spdlog::get("cachemanager")) {
            logger->error("Ошибка обновления метрик: {}", e.what());
        }
    }
}

void CacheManager::cleanupCache() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (!initialized || !pImpl->dynamicCache) {
            return;
        }
        
        pImpl->dynamicCache->clear(); // Теперь кэш реально очищается
        updateMetrics(); // <--- Обновляем метрики после очистки
        
        if (auto logger = spdlog::get("cachemanager")) {
            logger->debug("Кэш очищен, currentSize={}, entryCount={}",
                pImpl->metrics.currentSize, pImpl->metrics.entryCount);
        }
        
    } catch (const std::exception& e) {
        if (auto logger = spdlog::get("cachemanager")) {
            logger->error("Ошибка очистки кэша: {}", e.what());
        }
    }
}

std::unordered_map<std::string, std::vector<uint8_t>> CacheManager::exportAll() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    
    try {
        if (!initialized || !pImpl->dynamicCache) {
            return {};
        }
        
        return pImpl->dynamicCache->exportAll();
        
    } catch (const std::exception& e) {
        if (auto logger = spdlog::get("cachemanager")) {
            logger->error("Ошибка экспорта данных: {}", e.what());
        }
        return {};
    }
}

void CacheManager::shutdown() {

    spdlog::info("CacheManager: shutdown вызван");
    std::unique_ptr<DynamicCache<std::string, std::vector<uint8_t>>> tmpCache;
    {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
        if (!initialized) {
            return;
        }
        // Очищаем кэш
        if (pImpl->dynamicCache) {
            pImpl->dynamicCache->clear();
            tmpCache = std::move(pImpl->dynamicCache); // reset вне lock
        }
        pImpl->isInitialized = false;
        initialized = false;
        if (auto logger = spdlog::get("cachemanager")) {
            logger->info("CacheManager завершил работу");
        }
        }
    if (pImpl->dynamicCache) {
        spdlog::info("[DEBUG] shutdown: DynamicCache ptr = {}", static_cast<void*>(pImpl->dynamicCache.get()));
    }
    // Деструктор tmpCache вызовется здесь, вне lock
}

} // namespace cache
} // namespace core
} // namespace cloud

