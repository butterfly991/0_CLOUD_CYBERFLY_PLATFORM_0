# 🌐 Cloud IaaS Service — Обзор

> **Модульная C++20-платформа для облачных вычислений, балансировки, кэширования, восстановления и безопасности.**

---

## 🏗️ Архитектура (Mermaid)

```mermaid
graph TD
    A[Внешний клиент/API] --> B[OrchestrationKernel]
    B --> C[LoadBalancer]
    C --> D1[MicroKernel]
    C --> D2[SmartKernel]
    C --> D3[ParentKernel]
    D1 --> E1[DynamicCache]
    D2 --> E2[ThreadPool]
    D3 --> E3[RecoveryManager]
    D1 --> F1[SecurityManager]
    D2 --> F2[CryptoKernel]
    D3 --> F3[PreloadManager]
```

---

## 📦 Модули

| Модуль         | Описание                                   | Ключевые фичи           |
|----------------|--------------------------------------------|-------------------------|
| `balancer`     | Балансировка нагрузки                      | Гибридные стратегии     |
| `cache`        | Кэширование, динамика, предзагрузка        | LRU, TTL, Preload       |
| `drivers`      | Аппаратные драйверы (ARM/Apple)            | NEON/AMX, оптимизация   |
| `kernel`       | Ядра вычислений, иерархия                  | Расширяемость, PIMPL    |
| `recovery`     | Контрольные точки, восстановление          | Async, retention, метрики|
| `security`     | Безопасность, аудит, криптография          | Политики, OpenSSL, ARM  |
| `thread`       | Пул потоков, метрики, оптимизация          | Адаптация, логирование  |
| `utils`        | Вспомогательные утилиты                    |                         |

---

## 💡 Принципы дизайна

> - Минимализм, модульность, расширяемость
> - Thread Safety, RAII, PIMPL, DI
> - Метрики, аудит, тестируемость

---

## 🛠️ Технологии

- **C++20**, CMake, spdlog, OpenSSL, Boost, nlohmann/json, zlib
- Поддержка: macOS ARM, Linux x64

---

## ⚙️ Как это работает

- Задачи поступают в OrchestrationKernel → LoadBalancer выбирает ядро → задача исполняется в пуле потоков с кэшированием и аудитом.
- RecoveryManager обеспечивает отказоустойчивость, SecurityManager — безопасность.
- Все компоненты интегрированы через современные паттерны и метрики.

---

[⬅️ Назад к README](./README.md) | [Ядра и компоненты](./KERNELS_AND_COMPONENTS.md) 