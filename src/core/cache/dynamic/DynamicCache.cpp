#include "core/cache/dynamic/DynamicCache.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

namespace cloud {
namespace core {
namespace cache {

// Явная инстанциация для часто используемых типов
template class DynamicCache<std::string, std::vector<uint8_t>>;

} // namespace cache
} // namespace core
} // namespace cloud
