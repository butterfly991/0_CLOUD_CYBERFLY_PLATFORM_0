# ☁️ Cloud IaaS Service

> **Модульная облачная платформа на C++20 для высокопроизводительных вычислений, балансировки, кэширования, восстановления и безопасности.**

---

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B" />
  <img src="https://img.shields.io/badge/Platform-macOS%20ARM%20%7C%20Linux%20x64-green?logo=linux" />
  <img src="https://img.shields.io/badge/Build-CMake-informational?logo=cmake" />
  <img src="https://img.shields.io/badge/License-MIT-lightgrey" />
</p>

---

## 🚀 Быстрый старт

```sh
cmake -B build
cmake --build build -j
./build/bin/CloudIaaS
```

- Конфиг: `config/cloud_service.json`
- Запуск: `scripts/start_service.sh`

---

## 🧩 Архитектура

| Ядра                | Балансировка      | Кэш           | Восстановление | Безопасность   | Потоки         |
|---------------------|------------------|---------------|----------------|---------------|---------------|
| Core, Micro, Smart, Parent, Orchestration, Computational, Architectural, Crypto | Гибридные стратегии, метрики | DynamicCache, Preload, Adaptive | RecoveryManager, контрольные точки | SecurityManager, CryptoKernel | ThreadPool с оптимизациями |

<details>
<summary>Подробнее</summary>

- [Общее описание](./PROJECT_OVERVIEW.md)
- [Ядра и компоненты](./KERNELS_AND_COMPONENTS.md)
- [Безопасность](./SECURITY_DESIGN.md)
- [Процессы и потоки](./PROCESS_FLOW.md)

</details>

---

## ✨ Особенности

- Многоуровневая архитектура, гибкая модульность
- Поддержка ARM/Apple Silicon и Linux x64
- Гибридная балансировка, адаптивные кэши, recovery, аудит
- Современные паттерны: PIMPL, RAII, DI, Thread Safety
- Лаконичный API, расширяемость, тесты

---

## 📦 Структура проекта

```text
include/    # Заголовки (ядра, компоненты)
src/        # Исходный код
config/     # Конфиг (JSON)
logs/       # Логи
recovery_points/ # Контрольные точки
scripts/    # Скрипты запуска
tests/      # Тесты
```

---

## 🛠️ Ручная сборка и запуск

1. **Установите зависимости**
   - CMake (3.15+)
   - Компилятор C++20 (g++/clang++)
   - OpenSSL, Boost, zlib, spdlog, fmt, nlohmann/json

2. **Соберите проект**
   ```sh
   cmake -B build
   cmake --build build -j
   ```

3. **Запустите сервис**
   ```sh
   ./build/bin/CloudIaaS
   ```
   - Конфиг: `config/cloud_service.json`
   - Логи: `logs/`

---

## 🧪 Запуск тестов

1. **Соберите тесты**
   ```sh
   cmake -B build -DBUILD_TESTING=ON
   cmake --build build -j
   ```

2. **Запустите все тесты**
   ```sh
   cd build
   ctest --output-on-failure
   ```
   или вручную:
   ```sh
   ./bin/KernelSmokeTest
   ./bin/DynamicCacheSmokeTest
   ./bin/LoadBalancerSmokeTest
   # ... и другие тестовые бинарники из build/bin/
   ```

> Для macOS ARM используйте `brew install cmake openssl boost spdlog fmt nlohmann-json`.
> Для Linux — `apt install cmake g++ libssl-dev libboost-all-dev libspdlog-dev libfmt-dev nlohmann-json3-dev zlib1g-dev`.

---

## 📝 Пример использования

```cpp
#include <core/kernel/base/MicroKernel.hpp>

cloud::core::kernel::MicroKernel kernel("example");
kernel.initialize();
// ... задачи, интеграция с кэшем и балансировщиком ...
kernel.shutdown();
```

---

## 📚 Документация

- [Обзор и архитектура](./PROJECT_OVERVIEW.md)
- [Ядра и компоненты](./KERNELS_AND_COMPONENTS.md)
- [Безопасность](./SECURITY_DESIGN.md)
- [Процессы и потоки](./PROCESS_FLOW.md)
- [Performance Report](./PERFORMANCE_IMPROVEMENTS_REPORT.md)

---

## 🤝 Вклад и поддержка

- Вопросы и предложения — через Issues/Pull Requests
- Приветствуются любые улучшения и тесты!

---

<p align="center" style="font-size:1.35em; font-family:'Brush Script MT', cursive; color:#888; margin-top:2em;">
  <em>Cloud IaaS Service — современная C++ платформа для облачных вычислений.<br/>
  Open Source. Made with Cyberfly</em>
</p> 
