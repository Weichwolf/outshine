#ifndef OUTSHINE_GENERATORS_TERRAIN_GROUNDYIELD_H
#define OUTSHINE_GENERATORS_TERRAIN_GROUNDYIELD_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "GroundMesher.h"

namespace outshine {

inline constexpr double kBatterRise = 1.0 / 1.5;

inline constexpr uint32_t kNoStamp = 0xFFFFFFFFu;
inline constexpr uint32_t kHeldStamp = 0xFFFFFFFEu;

struct Covered {
  uint32_t Point = 0;
  uint32_t Stamp = 0;
};

struct Heights {
  std::span<const double> WrittenM;
  std::span<const double> WasM;
};

struct Floors {
  size_t Stamps = 0;
  size_t Unreached = 0;
  size_t Nodes = 0;
  size_t Contested = 0;
  double AboveM = 0.0;
  double BelowM = 0.0;
  double UnfilledM = 0.0;
  double WasAboveM = 0.0;
  double WasBelowM = 0.0;
};

struct Pressed {
  size_t Moved = 0;
  size_t Structures = 0;
  size_t Held = 0;
  std::vector<uint8_t> Refused;
  std::vector<uint32_t> DecidedBy;
  std::vector<Covered> Inside;
};

[[nodiscard]] Pressed PressPoints(std::span<const Yields> these,
                                  std::span<const EastSouth> at,
                                  std::span<double> upM,
                                  double mostEarthworkM);

[[nodiscard]] Floors FloorsOf(std::span<const Yields> these,
                              const Pressed &pressed,
                              Stamp kind,
                              std::span<const EastSouth> at,
                              Heights heights);

} // namespace outshine
#endif
