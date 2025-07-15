#!/bin/bash
# Скрипт для последовательного запуска всех тестовых бинарников из каталога build/

set -e
cd "$(dirname "$0")/../build"

# Список всех тестовых бинарников (обновлять при добавлении новых)
test_bins=(
  AdaptiveCacheSmokeTest
  ArchitecturalKernelSmokeTest
  ARMDriverSmokeTest
  CacheKernelIntegrationTest
  CacheManagerSmokeTest
  CacheSyncSmokeTest
  ComputationalKernelSmokeTest
  CoreKernelSmokeTest
  CryptoKernelSmokeTest
  CryptoMicroKernelSmokeTest
  DynamicCacheSmokeTest
  EnergyControllerSmokeTest
  FullSystemE2ETest
  KernelLoadBalancerIntegrationTest
  KernelSmokeTest
  LoadBalancerSmokeTest
  PlatformOptimizerSmokeTest
  PreloadManagerSmokeTest
  RecoveryManagerSmokeTest
  SecurityManagerSmokeTest
  SecurityRecoveryIntegrationTest
  SmartKernelSmokeTest
  TaskOrchestratorSmokeTest
  ThreadPoolSmokeTest
)

all_ok=1
for t in "${test_bins[@]}"; do
  if [[ -x "$t" ]]; then
    echo "===== $t ====="
    ./$t || all_ok=0
  else
    echo "[WARN] $t not found or not executable"
    all_ok=0
  fi
done

if [[ $all_ok -eq 1 ]]; then
  echo "[ALL TESTS PASSED]"
  exit 0
else
  echo "[SOME TESTS FAILED OR MISSING]"
  exit 1
fi 