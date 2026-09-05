#ifndef OUTSHINE_GENERATORS_TERRAIN_GROUNDYIELD_H
#define OUTSHINE_GENERATORS_TERRAIN_GROUNDYIELD_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "GroundMesher.h"

namespace outshine {

inline constexpr double kBatterRise = 1.0 / 1.5;

struct Pressed {
  size_t Moved = 0;
  size_t Structures = 0;
  size_t Held = 0;
};

[[nodiscard]] Pressed PressPoints(std::span<const Yields> these,
                                  std::span<const EastSouth> at,
                                  std::span<double> upM,
                                  double mostEarthworkM);

} // namespace outshine
#endif
