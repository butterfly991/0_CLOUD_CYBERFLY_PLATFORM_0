# 🧬 Ядра и компоненты Cloud IaaS Service

## ⚡ Ядра (Kernels)

| Ядро                | Описание                        | Ключевые фичи                |
|---------------------|----------------------------------|------------------------------|
| 🟦 CoreKernel       | Базовый абстрактный класс        | Интерфейс, PIMPL, метрики    |
| 🟩 MicroKernel      | Лёгкое ядро для задач            | Preload, LoadBalancer, Recovery |
| 🟨 SmartKernel      | Адаптивное интеллектуальное ядро | Мониторинг, адаптация, recovery |
| 🟧 ParentKernel     | Агрегатор дочерних ядер          | Балансировка, Energy, Orchestration |
| 🟪 OrchestrationKernel | Оркестрация и очереди           | Расширенные стратегии        |
| 🟥 ComputationalKernel | Вычислительные задачи           | DynamicCache, ARM, Recovery  |
| 🟫 ArchitecturalKernel | Архитектурные оптимизации       | ARM-драйверы, кэш            |
| 🟦 CryptoMicroKernel | Криптографические задачи         | ARM, DynamicCache, Recovery  |
| 🟦 CryptoKernel      | Криптографические операции       | ARM, CacheManager, DynamicCache |

---

## 🧩 Компоненты

| Компонент         | Описание                        | Ключевые фичи                |
|-------------------|----------------------------------|------------------------------|
| 🔄 LoadBalancer   | Гибридная балансировка           | Метрики, адаптация, логирование |
| ⚡ EnergyController | Энергоменеджмент                | Динамическое масштабирование |
| 🗂️ TaskOrchestrator | Очереди задач                   | Расширенные стратегии        |
| 🗃️ CacheManager   | Управление кэшем                 | Сериализация, метрики        |
| 🧠 DynamicCache   | Динамический кэш                 | LRU, TTL, авторасширение     |
| 🚀 PreloadManager | Предзагрузка данных              | ML-предсказания, async       |
| 💾 RecoveryManager | Контрольные точки, восстановление| Retention, метрики, PIMPL    |
| 🛡️ SecurityManager | Политики, аудит                  | Потокобезопасность           |
| 🧵 ThreadPool     | Пул потоков                      | Платформенные оптимизации    |

---

## 🔗 Взаимодействие

> Ядра используют LoadBalancer для распределения задач, кэширование реализовано через DynamicCache и CacheManager, RecoveryManager обеспечивает отказоустойчивость, SecurityManager и CryptoKernel — безопасность, ThreadPool — асинхронность и масштабируемость.

---

## 🧩 Архитектурные паттерны

> PIMPL, RAII, Dependency Injection, Thread Safety, Observer/Event, Singleton/Factory

---

[⬅️ К архитектуре](./PROJECT_OVERVIEW.md) | [Процессы и потоки](./PROCESS_FLOW.md) 