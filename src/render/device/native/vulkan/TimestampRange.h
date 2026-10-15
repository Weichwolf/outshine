#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_TIMESTAMPRANGE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_TIMESTAMPRANGE_H

#include <cmath>
#include <cstdint>
#include <limits>

namespace outshine::Render::Native::Vulkan {

[[nodiscard]] inline bool TimestampIntervalFits(uint64_t begin,
                                                uint64_t end,
                                                uint32_t bits,
                                                double nanosecondsPerTick,
                                                double hostUpperBoundNs) {
  if (bits == 0 || bits > 64 || !std::isfinite(nanosecondsPerTick) || nanosecondsPerTick <= 0 ||
      !std::isfinite(hostUpperBoundNs) || hostUpperBoundNs < 0 ||
      hostUpperBoundNs >= std::ldexp(nanosecondsPerTick, static_cast<int>(bits))) {
    return false;
  }
  const auto mask = std::numeric_limits<uint64_t>::max() >> (64 - bits);
  return static_cast<double>((end - begin) & mask) * nanosecondsPerTick <= hostUpperBoundNs;
}

}
#endif
