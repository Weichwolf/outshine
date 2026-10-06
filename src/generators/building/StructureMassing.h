#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREMASSING_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREMASSING_H

#include "StructureBake.h"

#include <atomic>
#include <array>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators {

struct StructureMassPlan {
  double LowLat = 0.0, HighLat = 0.0, LowLon = 0.0, HighLon = 0.0;
  double BaseSum = 0.0, SeatSum = 0.0, HeightSum = 0.0;
  double MinimumBaseM = 0.0, MaximumTopM = 0.0;
  int Count = 0;
  uint32_t Cell = 0;
  double PitchedAreaM2 = 0.0, RoofAreaM2 = 0.0;
  Vec3 WallColourSum{};
  bool HasWallColour = false;
  LevelOfDetail Level = LevelOfDetail::Massed;
};

[[nodiscard]] StructurePlan DescribeStructureMass(const StructureMassPlan &mass,
                                                  const RawTile &raw,
                                                  std::array<double, 8> &ring,
                                                  std::array<double, 4> &corners);

[[nodiscard]] StructureMassPlan CombineStructureMasses(std::span<const StructureMassPlan> plans,
                                                       std::span<const uint32_t> members);

[[nodiscard]] std::expected<std::vector<StructureMassPlan>, StructureBakeError>
GroupStructureMasses(std::span<StructureMassPlan> plans,
                     const RawTile &raw,
                     const std::atomic_bool *stopping);

}
#endif
