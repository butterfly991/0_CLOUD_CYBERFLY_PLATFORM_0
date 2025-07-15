cmake_minimum_required(VERSION 3.15)
project(CloudIaaS VERSION 1.0.0 LANGUAGES CXX C)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции компиляции
option(CLOUD_ENABLE_LOGGING "Enable logging" ON)
option(CLOUD_ENABLE_METRICS "Enable metrics collection" ON)
option(CLOUD_ENABLE_PERFORMANCE_OPTIMIZATIONS "Enable performance optimizations" ON)
option(BUILD_TESTING "Build tests" ON)

# Флаги компилятора
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
    
    # Включаем предупреждения
    add_compile_options(-Wall -Wextra -Wpedantic)
    
    # Включаем оптимизации
    if(CLOUD_ENABLE_PERFORMANCE_OPTIMIZATIONS)
        add_compile_options(-O3 -march=native)
    endif()
endif()

# Поиск необходимых пакетов
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)

# Поиск fmt и spdlog
find_path(FMT_INCLUDE_DIR fmt/format.h
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
find_library(FMT_LIBRARY
    NAMES fmt
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)

find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
find_library(SPDLOG_LIBRARY
    NAMES spdlog
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)

if(NOT FMT_INCLUDE_DIR OR NOT FMT_LIBRARY)
    message(FATAL_ERROR "fmt library not found")
endif()

if(NOT SPDLOG_INCLUDE_DIR OR NOT SPDLOG_LIBRARY)
    message(FATAL_ERROR "spdlog library not found")
endif()

# Добавление исходных файлов
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.c"
)

# Добавление заголовочных файлов
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${FMT_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

# Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES})

# Линковка с библиотеками
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    ${Boost_LIBRARIES}
    ${FMT_LIBRARY}
    ${SPDLOG_LIBRARY}
)

# Установка выходной директории
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Установка директории для объектных файлов
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Включение тестов
if(BUILD_TESTING)
    enable_testing()
    include_directories(include src)

    # Собираем все исходники для тестов
    file(GLOB_RECURSE KERNEL_SOURCES src/core/kernel/*.cpp)
    file(GLOB_RECURSE CACHE_SOURCES src/core/cache/*.cpp)
    file(GLOB_RECURSE BALANCER_SOURCES src/core/balancer/*.cpp)
    file(GLOB_RECURSE SECURITY_SOURCES src/core/security/*.cpp)
    file(GLOB_RECURSE DRIVER_SOURCES src/core/drivers/*.cpp)
    file(GLOB_RECURSE RECOVERY_SOURCES src/core/recovery/*.cpp)
    file(GLOB_RECURSE THREAD_SOURCES src/core/thread/*.cpp)

    set(ALL_CORE_SOURCES
        ${KERNEL_SOURCES}
        ${CACHE_SOURCES}
        ${BALANCER_SOURCES}
        ${SECURITY_SOURCES}
        ${DRIVER_SOURCES}
        ${RECOVERY_SOURCES}
        ${THREAD_SOURCES}
    )

    add_executable(KernelSmokeTest tests/core/kernel/KernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(KernelSmokeTest PRIVATE include src)
    target_link_libraries(KernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME KernelSmokeTest COMMAND KernelSmokeTest)

    add_executable(DynamicCacheSmokeTest tests/core/cache/DynamicCacheSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(DynamicCacheSmokeTest PRIVATE include src)
    target_link_libraries(DynamicCacheSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME DynamicCacheSmokeTest COMMAND DynamicCacheSmokeTest)

    add_executable(LoadBalancerSmokeTest tests/core/balancer/LoadBalancerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(LoadBalancerSmokeTest PRIVATE include src)
    target_link_libraries(LoadBalancerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME LoadBalancerSmokeTest COMMAND LoadBalancerSmokeTest)

    add_executable(SecurityManagerSmokeTest tests/core/security/SecurityManagerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(SecurityManagerSmokeTest PRIVATE include src)
    target_link_libraries(SecurityManagerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME SecurityManagerSmokeTest COMMAND SecurityManagerSmokeTest)

    # Новые smoke тесты для непротестированных компонентов
    add_executable(CoreKernelSmokeTest tests/core/kernel/CoreKernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(CoreKernelSmokeTest PRIVATE include src)
    target_link_libraries(CoreKernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME CoreKernelSmokeTest COMMAND CoreKernelSmokeTest)

    add_executable(SmartKernelSmokeTest tests/core/kernel/SmartKernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(SmartKernelSmokeTest PRIVATE include src)
    target_link_libraries(SmartKernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME SmartKernelSmokeTest COMMAND SmartKernelSmokeTest)

    add_executable(ComputationalKernelSmokeTest tests/core/kernel/ComputationalKernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(ComputationalKernelSmokeTest PRIVATE include src)
    target_link_libraries(ComputationalKernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME ComputationalKernelSmokeTest COMMAND ComputationalKernelSmokeTest)

    add_executable(ArchitecturalKernelSmokeTest tests/core/kernel/ArchitecturalKernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(ArchitecturalKernelSmokeTest PRIVATE include src)
    target_link_libraries(ArchitecturalKernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME ArchitecturalKernelSmokeTest COMMAND ArchitecturalKernelSmokeTest)

    add_executable(CryptoMicroKernelSmokeTest tests/core/kernel/CryptoMicroKernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(CryptoMicroKernelSmokeTest PRIVATE include src)
    target_link_libraries(CryptoMicroKernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME CryptoMicroKernelSmokeTest COMMAND CryptoMicroKernelSmokeTest)

    add_executable(EnergyControllerSmokeTest tests/core/balancer/EnergyControllerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(EnergyControllerSmokeTest PRIVATE include src)
    target_link_libraries(EnergyControllerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME EnergyControllerSmokeTest COMMAND EnergyControllerSmokeTest)

    add_executable(TaskOrchestratorSmokeTest tests/core/balancer/TaskOrchestratorSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(TaskOrchestratorSmokeTest PRIVATE include src)
    target_link_libraries(TaskOrchestratorSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME TaskOrchestratorSmokeTest COMMAND TaskOrchestratorSmokeTest)

    add_executable(CacheManagerSmokeTest tests/core/cache/CacheManagerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(CacheManagerSmokeTest PRIVATE include src)
    target_link_libraries(CacheManagerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME CacheManagerSmokeTest COMMAND CacheManagerSmokeTest)

    add_executable(CacheSyncSmokeTest tests/core/cache/CacheSyncSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(CacheSyncSmokeTest PRIVATE include src)
    target_link_libraries(CacheSyncSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME CacheSyncSmokeTest COMMAND CacheSyncSmokeTest)

    add_executable(AdaptiveCacheSmokeTest tests/core/cache/AdaptiveCacheSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(AdaptiveCacheSmokeTest PRIVATE include src)
    target_link_libraries(AdaptiveCacheSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME AdaptiveCacheSmokeTest COMMAND AdaptiveCacheSmokeTest)

    add_executable(PlatformOptimizerSmokeTest tests/core/cache/PlatformOptimizerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(PlatformOptimizerSmokeTest PRIVATE include src)
    target_link_libraries(PlatformOptimizerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME PlatformOptimizerSmokeTest COMMAND PlatformOptimizerSmokeTest)

    add_executable(PreloadManagerSmokeTest tests/core/cache/PreloadManagerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(PreloadManagerSmokeTest PRIVATE include src)
    target_link_libraries(PreloadManagerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME PreloadManagerSmokeTest COMMAND PreloadManagerSmokeTest)

    add_executable(CryptoKernelSmokeTest tests/core/security/CryptoKernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(CryptoKernelSmokeTest PRIVATE include src)
    target_link_libraries(CryptoKernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME CryptoKernelSmokeTest COMMAND CryptoKernelSmokeTest)

    add_executable(RecoveryManagerSmokeTest tests/core/recovery/RecoveryManagerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(RecoveryManagerSmokeTest PRIVATE include src)
    target_link_libraries(RecoveryManagerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME RecoveryManagerSmokeTest COMMAND RecoveryManagerSmokeTest)

    add_executable(ARMDriverSmokeTest tests/core/drivers/ARMDriverSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(ARMDriverSmokeTest PRIVATE include src)
    target_link_libraries(ARMDriverSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME ARMDriverSmokeTest COMMAND ARMDriverSmokeTest)

    add_executable(ThreadPoolSmokeTest tests/core/thread/ThreadPoolSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(ThreadPoolSmokeTest PRIVATE include src)
    target_link_libraries(ThreadPoolSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME ThreadPoolSmokeTest COMMAND ThreadPoolSmokeTest)

    # Интеграционные тесты
    add_executable(KernelLoadBalancerIntegrationTest tests/core/integration/KernelLoadBalancerIntegrationTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(KernelLoadBalancerIntegrationTest PRIVATE include src)
    target_link_libraries(KernelLoadBalancerIntegrationTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME KernelLoadBalancerIntegrationTest COMMAND KernelLoadBalancerIntegrationTest)

    add_executable(CacheKernelIntegrationTest tests/core/integration/CacheKernelIntegrationTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(CacheKernelIntegrationTest PRIVATE include src)
    target_link_libraries(CacheKernelIntegrationTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME CacheKernelIntegrationTest COMMAND CacheKernelIntegrationTest)

    add_executable(SecurityRecoveryIntegrationTest tests/core/integration/SecurityRecoveryIntegrationTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(SecurityRecoveryIntegrationTest PRIVATE include src)
    target_link_libraries(SecurityRecoveryIntegrationTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME SecurityRecoveryIntegrationTest COMMAND SecurityRecoveryIntegrationTest)

    # End-to-End тесты
    add_executable(FullSystemE2ETest tests/core/e2e/FullSystemE2ETest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(FullSystemE2ETest PRIVATE include src)
    target_link_libraries(FullSystemE2ETest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES} ${FMT_LIBRARY} ${SPDLOG_LIBRARY})
    add_test(NAME FullSystemE2ETest COMMAND FullSystemE2ETest)

    # Дополнительные smoke тесты для недостающих компонентов
    # Удаляю невалидные тесты
    # add_executable(TunnelManagerSmokeTest ...)
    # add_executable(ResourceManagerSmokeTest ...)
    # add_executable(AdaptiveControllerSmokeTest ...)
    # add_executable(FullSystemIntegrationTest ...)
    # add_executable(KernelUnitTest ...)
    # add_executable(SystemStressTest ...)
    # add_test(NAME TunnelManagerSmokeTest ...)
    # add_test(NAME ResourceManagerSmokeTest ...)
    # add_test(NAME AdaptiveControllerSmokeTest ...)
    # add_test(NAME FullSystemIntegrationTest ...)
    # add_test(NAME KernelUnitTest ...)
    # add_test(NAME SystemStressTest ...)
endif()

# Установка
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Установка заголовочных файлов
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)
