#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <algorithm>
#include <list>
#include <spdlog/spdlog.h>
#include <initializer_list>
#include <optional>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <sstream>

namespace cloud {
namespace core {
namespace cache {

// DynamicCache — потокобезопасный динамический кэш с TTL, автоочисткой и интеграцией
// Используется для ускорения доступа к данным и оптимизации под платформу
// Поддерживает eviction, автоresize, batch, sync, миграцию
template<typename Key, typename Value>
class DynamicCache {
public:
    using KeyType = Key;
    using DataType = Value;
    using EvictionCallback = std::function<void(const Key&, const Value&)>;
    using Clock = std::chrono::steady_clock;
    struct Entry {
        DataType data;
        Clock::time_point lastAccess;
        size_t ttlSeconds; // TTL (0 = бессрочно)
    };
    DynamicCache(size_t initialSize, size_t defaultTTL = 0); // Конструктор
    ~DynamicCache(); // Деструктор
    std::optional<Value> get(const Key& key); // Получить
    void put(const Key& key, const Value& value); // Сохранить
    void put(const Key& key, const Value& value, size_t ttlSeconds); // Сохранить с TTL
    void remove(const Key& key); // Удалить
    void clear(); // Очистить
    size_t size() const; // Размер
    size_t allocatedSize() const; // Выделено
    void resize(size_t newSize); // Изменить размер
    void setEvictionCallback(EvictionCallback cb); // Callback вытеснения
    void setAutoResize(bool enable, size_t minSize, size_t maxSize); // Авторазмер
    void setCleanupInterval(size_t seconds); // Интервал очистки
    void batchPut(const std::unordered_map<KeyType, DataType>& data, size_t ttlSeconds = 0); // Массовое добавление
    void syncWith(const DynamicCache& other); // Синхронизация
    void migrateTo(DynamicCache& target) const; // Миграция
    std::unordered_map<Key, Value> exportAll() const; // Экспорт
    void cleanupSync(); // Синхр. очистка
private:
    void evictIfNeeded();
    void cleanupThreadFunc();
    void evictLRU();
    void autoResize();
    void removeExpired();
    void startCleanupThread();
    void stopCleanupThread();
    void notifyCleanupThread();
    size_t allocatedSize_;
    size_t defaultTTL_;
    std::unordered_map<KeyType, std::pair<typename std::list<KeyType>::iterator, Entry>> cache_;
    std::list<KeyType> lruList_;
    mutable std::shared_mutex mutex_;
    EvictionCallback evictionCallback_;
    std::thread cleanupThread_;
    std::atomic<bool> stopCleanup_{false};
    std::atomic<bool> cleanupThreadRunning_{false};
    std::atomic<bool> cleanupThreadShouldExit_{false};
    size_t cleanupIntervalSeconds_ = 5;
    bool autoResizeEnabled_ = false;
    size_t minSize_ = 16;
    size_t maxSize_ = 4096;
    std::condition_variable_any cleanupCv_;
    std::atomic<size_t> totalOperations_{0};
    std::atomic<size_t> cacheHits_{0};
    std::atomic<size_t> cacheMisses_{0};
    std::atomic<size_t> evictions_{0};
    Clock::time_point lastCleanupTime_{Clock::now()};
    Clock::time_point lastOperationTime_{Clock::now()};
    bool shouldRunCleanup() const;
    size_t calculateOptimalCleanupInterval() const;
    void updateMetrics(bool hit);
    void logPerformanceMetrics() const;
    bool isIdle() const;
    void adaptiveSleep();
<<<<<<< HEAD
=======
    std::mutex cleanupMutex_; // Новый mutex только для ожидания на cleanupCv_
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
};

// Алиас для удобства использования динамического кэша по умолчанию
using DefaultDynamicCache = DynamicCache<std::string, std::vector<uint8_t>>;

// --- Константы для DynamicCache ---
constexpr size_t MIN_CLEANUP_INTERVAL = 1;
constexpr size_t MAX_CLEANUP_INTERVAL = 60;
constexpr size_t IDLE_SLEEP_SECONDS = 10;
constexpr size_t CLEANUP_TIMEOUT_SECONDS = 30;

} // namespace cache
} // namespace core
} // namespace cloud

// Реализация шаблонного класса
#include <optional>
#include <thread>
#include <condition_variable>

namespace cloud {
namespace core {
namespace cache {

template<typename Key, typename Value>
DynamicCache<Key, Value>::DynamicCache(size_t initialSize, size_t defaultTTL)
    : allocatedSize_(initialSize), defaultTTL_(defaultTTL) {
    
    // Оптимизированная инициализация
    cache_.reserve(initialSize);
    // std::list не поддерживает reserve, убираем эту строку
    
    // Настройка адаптивного интервала очистки
    cleanupIntervalSeconds_ = std::max(MIN_CLEANUP_INTERVAL, 
                                      std::min(MAX_CLEANUP_INTERVAL, 
                                              static_cast<size_t>(initialSize / 100)));
    
    // Запуск фонового потока только если нужно
    if (initialSize > 0) {
        startCleanupThread();
    }
    
    spdlog::info("DynamicCache: создан с параметрами: initialSize={}, defaultTTL={}, cleanupInterval={}s (адаптивный)", 
                 initialSize, defaultTTL, cleanupIntervalSeconds_);
}

template<typename Key, typename Value>
DynamicCache<Key, Value>::~DynamicCache() {
<<<<<<< HEAD
=======
    spdlog::info("DynamicCache: деструктор вызван, завершаем cleanupThread...");
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
    stopCleanupThread();
}

template<typename Key, typename Value>
std::optional<Value> DynamicCache<Key, Value>::get(const Key& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        updateMetrics(false);
        return std::nullopt;
    }
    
    // Проверка TTL
    if (it->second.second.ttlSeconds > 0) {
        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.second.lastAccess).count();
        if (static_cast<size_t>(elapsed) >= it->second.second.ttlSeconds) {
            // Запись истекла, удаляем её
            lock.unlock();
            remove(key);
            updateMetrics(false);
            return std::nullopt;
        }
    }
    
    // Обновляем LRU
    lruList_.splice(lruList_.begin(), lruList_, it->second.first);
    it->second.second.lastAccess = Clock::now();
    
    updateMetrics(true);
    return it->second.second.data;
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::put(const Key& key, const Value& value) {
    put(key, value, defaultTTL_);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::put(const Key& key, const Value& value, size_t ttlSeconds) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // Обновляем существующую запись
        it->second.second.data = value;
        it->second.second.lastAccess = Clock::now();
        it->second.second.ttlSeconds = ttlSeconds;
        
        // Перемещаем в начало LRU списка
        lruList_.splice(lruList_.begin(), lruList_, it->second.first);
    } else {
        // Добавляем новую запись
        // Принудительно вытесняем один элемент, если кэш достиг лимита
        if (cache_.size() >= allocatedSize_) {
            evictLRU(); // Вытесняем один элемент
        }
        
        // Добавляем в LRU список
        lruList_.push_front(key);
        
        // Добавляем в кэш
        cache_[key] = std::make_pair(lruList_.begin(), Entry{value, Clock::now(), ttlSeconds});
    }
    
    totalOperations_.fetch_add(1, std::memory_order_relaxed);
    lastOperationTime_ = Clock::now();
    
    // Уведомляем поток очистки
    cleanupCv_.notify_one();
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::remove(const Key& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (evictionCallback_) {
            evictionCallback_(key, it->second.second.data);
        }
        lruList_.erase(it->second.first);
        cache_.erase(it);
    }
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (evictionCallback_) {
        for (const auto& [key, entry] : cache_) {
            evictionCallback_(key, entry.second.data);
        }
    }
    
    cache_.clear();
    lruList_.clear();
}

template<typename Key, typename Value>
size_t DynamicCache<Key, Value>::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return cache_.size();
}

template<typename Key, typename Value>
size_t DynamicCache<Key, Value>::allocatedSize() const {
    return allocatedSize_;
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::resize(size_t newSize) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (newSize < cache_.size()) {
        // Уменьшаем размер, вытесняем лишние записи
        size_t toEvict = cache_.size() - newSize;
        for (size_t i = 0; i < toEvict && !lruList_.empty(); ++i) {
            const auto& key = lruList_.back();
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                if (evictionCallback_) {
                    evictionCallback_(key, it->second.second.data);
                }
                cache_.erase(it);
                lruList_.pop_back();
                evictions_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    
    allocatedSize_ = newSize;
    spdlog::debug("DynamicCache: изменён размер на {} записей", newSize);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::setEvictionCallback(EvictionCallback cb) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    evictionCallback_ = std::move(cb);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::setAutoResize(bool enable, size_t minSize, size_t maxSize) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    autoResizeEnabled_ = enable;
    minSize_ = minSize;
    maxSize_ = maxSize;
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::setCleanupInterval(size_t seconds) {
    cleanupIntervalSeconds_ = std::max(MIN_CLEANUP_INTERVAL, 
                                      std::min(MAX_CLEANUP_INTERVAL, seconds));
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::batchPut(const std::unordered_map<KeyType, DataType>& data, size_t ttlSeconds) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    for (const auto& [key, value] : data) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second.second.data = value;
            it->second.second.lastAccess = Clock::now();
            it->second.second.ttlSeconds = ttlSeconds;
            lruList_.splice(lruList_.begin(), lruList_, it->second.first);
        } else {
            if (cache_.size() >= allocatedSize_) {
                // Вытесняем одну запись для новой
                if (!lruList_.empty()) {
                    const auto& oldKey = lruList_.back();
                    auto oldIt = cache_.find(oldKey);
                    if (oldIt != cache_.end()) {
                        if (evictionCallback_) {
                            evictionCallback_(oldKey, oldIt->second.second.data);
                        }
                        cache_.erase(oldIt);
                        lruList_.pop_back();
                        evictions_.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
            
            lruList_.push_front(key);
            cache_[key] = {lruList_.begin(), {value, Clock::now(), ttlSeconds}};
        }
    }
    
    totalOperations_.fetch_add(data.size(), std::memory_order_relaxed);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::syncWith(const DynamicCache& other) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::shared_lock<std::shared_mutex> otherLock(other.mutex_);
    
    // Очищаем текущий кэш
    if (evictionCallback_) {
        for (const auto& [key, entry] : cache_) {
            evictionCallback_(key, entry.second.data);
        }
    }
    
    cache_.clear();
    lruList_.clear();
    
    // Копируем данные из другого кэша
    for (const auto& [key, entry] : other.cache_) {
        lruList_.push_back(key);
        auto listIt = --lruList_.end();
        cache_[key] = {listIt, entry.second};
    }
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::migrateTo(DynamicCache& target) const {
    target.syncWith(*this);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::evictIfNeeded() {
    while (cache_.size() > allocatedSize_ && !lruList_.empty()) {
        evictLRU();
    }
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::evictLRU() {
    if (lruList_.empty()) return;
    
    const auto& key = lruList_.back();
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (evictionCallback_) {
            evictionCallback_(key, it->second.second.data);
        }
        cache_.erase(it);
        lruList_.pop_back();
        evictions_.fetch_add(1, std::memory_order_relaxed);
    }
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::startCleanupThread() {
    if (cleanupThreadRunning_.load(std::memory_order_acquire)) {
        return;
    }
    
    stopCleanup_.store(false, std::memory_order_release);
    cleanupThreadRunning_.store(true, std::memory_order_release);
    
    cleanupThread_ = std::thread([this] {
        cleanupThreadFunc();
    });
    
    spdlog::debug("DynamicCache: фоновый поток запущен с интервалом {} секунд", cleanupIntervalSeconds_);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::stopCleanupThread() {
<<<<<<< HEAD
    if (!cleanupThreadRunning_.load(std::memory_order_acquire)) {
        return;
    }
    
    stopCleanup_.store(true, std::memory_order_release);
    cleanupCv_.notify_all();
    
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
    
    cleanupThreadRunning_.store(false, std::memory_order_release);
    spdlog::debug("DynamicCache: фоновый поток завершен (адаптивный)");
=======
    spdlog::info("DynamicCache: stopCleanupThread вызван");
    if (!cleanupThreadRunning_.load(std::memory_order_acquire)) {
        spdlog::info("DynamicCache: cleanupThread уже не работает");
        return;
    }
    stopCleanup_.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(cleanupMutex_);
        cleanupCv_.notify_all();
    }
    if (cleanupThread_.joinable()) {
        spdlog::info("DynamicCache: ожидаем завершения cleanupThread...");
        cleanupThread_.join();
        spdlog::info("DynamicCache: cleanupThread завершён");
    }
    cleanupThreadRunning_.store(false, std::memory_order_release);
    spdlog::info("DynamicCache: stopCleanupThread завершён");
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::notifyCleanupThread() {
    cleanupCv_.notify_one();
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::updateMetrics(bool hit) {
    if (hit) {
        cacheHits_.fetch_add(1, std::memory_order_relaxed);
    } else {
        cacheMisses_.fetch_add(1, std::memory_order_relaxed);
    }
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::logPerformanceMetrics() const {
    auto hits = cacheHits_.load(std::memory_order_relaxed);
    auto misses = cacheMisses_.load(std::memory_order_relaxed);
    auto total = hits + misses;
    auto hitRate = total > 0 ? static_cast<double>(hits) / total : 0.0;
    
    spdlog::debug("DynamicCache: метрики - размер={}, попадания={}, промахи={}, hitRate={:.2f}%", 
                  cache_.size(), hits, misses, hitRate * 100);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::autoResize() {
    spdlog::debug("DynamicCache: autoResize ENTER, currentSize={}, allocatedSize_={}", 
                  cache_.size(), allocatedSize_);
    
    auto targetSize = allocatedSize_;
    
    // Если кэш почти пуст, уменьшаем размер
    if (cache_.size() < allocatedSize_ * 0.25 && allocatedSize_ > minSize_) {
        targetSize = std::max(minSize_, allocatedSize_ / 2);
    }
    // Если кэш почти полон, увеличиваем размер
    else if (cache_.size() > allocatedSize_ * 0.8 && allocatedSize_ < maxSize_) {
        targetSize = std::min(maxSize_, allocatedSize_ * 2);
    }
    
    if (targetSize != allocatedSize_) {
        allocatedSize_ = targetSize;
        spdlog::debug("DynamicCache: auto-resized to {} entries", allocatedSize_);
    }
    
    spdlog::debug("DynamicCache: autoResize EXIT, allocatedSize_={}", allocatedSize_);
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::removeExpired() {
    auto now = Clock::now();
    auto it = lruList_.begin();
    
    while (it != lruList_.end()) {
        auto cacheIt = cache_.find(*it);
        if (cacheIt != cache_.end() && cacheIt->second.second.ttlSeconds > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - cacheIt->second.second.lastAccess).count();
            if (static_cast<size_t>(elapsed) >= cacheIt->second.second.ttlSeconds) {
                if (evictionCallback_) {
                    evictionCallback_(*it, cacheIt->second.second.data);
                }
                cache_.erase(cacheIt);
                it = lruList_.erase(it);
                evictions_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
        }
        ++it;
    }
}

template<typename Key, typename Value>
bool DynamicCache<Key, Value>::shouldRunCleanup() const {
    return !stopCleanup_.load(std::memory_order_acquire) && 
           (cache_.size() > minSize_ || allocatedSize_ == minSize_);
}

template<typename Key, typename Value>
size_t DynamicCache<Key, Value>::calculateOptimalCleanupInterval() const {
    auto size = cache_.size();
    auto operations = totalOperations_.load(std::memory_order_relaxed);
    
    // Адаптивный интервал на основе размера и активности
    if (size == 0) return MAX_CLEANUP_INTERVAL;
    if (operations > 1000) return MIN_CLEANUP_INTERVAL;
    if (size > allocatedSize_ * 0.8) return MIN_CLEANUP_INTERVAL;
    
    return std::max(MIN_CLEANUP_INTERVAL, 
                   std::min(MAX_CLEANUP_INTERVAL, cleanupIntervalSeconds_));
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::adaptiveSleep() {
<<<<<<< HEAD
=======
    if (stopCleanup_.load(std::memory_order_acquire)) {
        spdlog::info("DynamicCache: adaptiveSleep: stopCleanup_ выставлен, не засыпаем");
        return;
    }
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
    auto interval = calculateOptimalCleanupInterval();
    
    if (isIdle()) {
        // Если кэш неактивен, спим дольше
        spdlog::info("DynamicCache: кэш неактивен, сон {} секунд", IDLE_SLEEP_SECONDS);
<<<<<<< HEAD
        std::unique_lock<std::shared_mutex> lock(mutex_);
=======
        std::unique_lock<std::mutex> lock(cleanupMutex_);
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
        cleanupCv_.wait_for(lock, std::chrono::seconds(IDLE_SLEEP_SECONDS), 
                           [this] { return stopCleanup_.load(std::memory_order_acquire); });
    } else {
        // Обычный сон с уведомлениями
<<<<<<< HEAD
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cleanupCv_.wait_for(lock, std::chrono::seconds(interval), 
                           [this] { return stopCleanup_.load(std::memory_order_acquire); });
    }
=======
        std::unique_lock<std::mutex> lock(cleanupMutex_);
        cleanupCv_.wait_for(lock, std::chrono::seconds(interval), 
                           [this] { return stopCleanup_.load(std::memory_order_acquire); });
    }
    // Явная проверка после выхода из wait_for
    if (stopCleanup_.load(std::memory_order_acquire)) {
        spdlog::info("DynamicCache: adaptiveSleep: stopCleanup_ выставлен после wait, выходим");
        return;
    }
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
}

template<typename Key, typename Value>
bool DynamicCache<Key, Value>::isIdle() const {
    auto now = Clock::now();
    auto timeSinceLastOp = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastCleanupTime_).count();
    return cache_.size() <= minSize_ && static_cast<size_t>(timeSinceLastOp) > CLEANUP_TIMEOUT_SECONDS;
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::cleanupThreadFunc() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    spdlog::info("DynamicCache: cleanupThread стартует (thread_id={})", oss.str());
    
    while (!stopCleanup_.load(std::memory_order_acquire)) {
<<<<<<< HEAD
=======
        spdlog::info("DynamicCache: cleanupThreadFunc цикл, stopCleanup_={}", stopCleanup_.load());
>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
        // Выполняем очистку
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            if (stopCleanup_.load(std::memory_order_acquire)) break;
            
            removeExpired();
            evictIfNeeded();
            
            if (autoResizeEnabled_) {
                autoResize();
            }
        }
        
        lastCleanupTime_ = Clock::now();
        // Убираем неиспользуемую переменную cleanupDuration
        
        // Адаптивный сон
        adaptiveSleep();
        
<<<<<<< HEAD
=======
        // Явная проверка после adaptiveSleep
        if (stopCleanup_.load(std::memory_order_acquire)) break;

>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
        // Логируем метрики каждые 10 операций
        if (totalOperations_.load(std::memory_order_relaxed) % 10 == 0) {
            logPerformanceMetrics();
        }
    }
    
    spdlog::info("DynamicCache: cleanupThread полностью завершён (thread_id={})", oss.str());
}

<<<<<<< HEAD
=======
template<typename Key, typename Value>
std::unordered_map<Key, Value> DynamicCache<Key, Value>::exportAll() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::unordered_map<Key, Value> result;
    for (const auto& [key, pair] : cache_) {
        result[key] = pair.second.data;
    }
    return result;
}

template<typename Key, typename Value>
void DynamicCache<Key, Value>::cleanupSync() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    removeExpired();
    evictIfNeeded();
}

>>>>>>> 6194c3d (Аудит, исправления потоков, автоматизация тестов: добавлен run_all_tests.sh, исправлены deadlock-и, все тесты проходят)
} // namespace cache
} // namespace core
} // namespace cloud
