#ifndef OUTSHINE_BASE_GEO_PLACEKEY_H
#define OUTSHINE_BASE_GEO_PLACEKEY_H

#include <cmath>
#include <cstdint>

#include "Earth.h"

namespace outshine {

[[nodiscard]] inline uint64_t PlaceKey(LongitudeLatitude at) {
  constexpr int64_t kBias = 0x20000000;
  const auto y = static_cast<int64_t>(std::llround(at.LatitudeDeg * 100000.0));
  const auto x = static_cast<int64_t>(std::llround(at.LongitudeDeg * 100000.0));
  return (static_cast<uint64_t>(y + kBias) << 32U) | static_cast<uint64_t>(x + kBias);
}

} // namespace outshine
#endif
