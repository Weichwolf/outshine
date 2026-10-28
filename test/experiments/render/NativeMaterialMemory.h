#ifndef OUTSHINE_TEST_EXPERIMENTS_RENDER_NATIVEMATERIALMEMORY_H
#define OUTSHINE_TEST_EXPERIMENTS_RENDER_NATIVEMATERIALMEMORY_H

#include <cstdint>
#include <optional>

namespace outshine::Test {
#ifdef __APPLE__
[[nodiscard]] std::optional<uint64_t> MaterialDeviceAllocatedBytes() noexcept;
#else
[[nodiscard]] inline std::optional<uint64_t> MaterialDeviceAllocatedBytes() noexcept {
  return std::nullopt;
}
#endif
}
#endif
