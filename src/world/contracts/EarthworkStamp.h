#ifndef OUTSHINE_WORLD_CONTRACTS_EARTHWORKSTAMP_H
#define OUTSHINE_WORLD_CONTRACTS_EARTHWORKSTAMP_H

#include "Earth.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace outshine {
enum class EarthworkKind : uint8_t { Pad, Corridor, Basin, Clearance };

struct ProfiledCorridorSpan {
  uint64_t CorridorKey = 0;
  EastNorth BeginM;
  EastNorth EndM;
  EastNorthUp BeginDerivativeM;
  EastNorthUp EndDerivativeM;
  double StationLengthM = 0.0;
  double BeginBedM = 0.0;
  double EndBedM = 0.0;
  double BeginPavementHalfWidthM = 0.0;
  double EndPavementHalfWidthM = 0.0;
  double BeginHalfWidthM = 0.0;
  double EndHalfWidthM = 0.0;

  [[nodiscard]] bool operator==(const ProfiledCorridorSpan &) const = default;
};

struct EarthworkStamp {
  std::vector<double> RingEastNorthM;
  std::vector<std::vector<double>> HoleRingsEastNorthM;
  double LowE = 0.0, HighE = 0.0, LowN = 0.0, HighN = 0.0;
  double AtE = 0.0, AtN = 0.0;
  double PlateauM = 0.0;
  double SlopeE = 0.0, SlopeN = 0.0;
  std::vector<double> SeamEastNorthM;
  std::optional<ProfiledCorridorSpan> Profile;
  std::optional<double> PlanarSupportM;
  uint64_t CorridorKey = 0;
  double ApronM = 0.0;
  double YieldM = 0.0;
  double SagInv = 0.0;
  bool Fills = false;
  EarthworkKind Kind = EarthworkKind::Pad;

  [[nodiscard]] bool operator==(const EarthworkStamp &) const = default;

  [[nodiscard]] size_t HeapBytes() const noexcept;

  [[nodiscard]] double WantsAt(EastNorth at) const;
};

}
#endif
