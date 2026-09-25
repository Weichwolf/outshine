#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURECELL_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURECELL_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "math/Units.h"
#include "StructureCellGrid.h"
#include "TileGeodesy.h"

namespace outshine::Generators {

inline constexpr uint32_t kStructureCellSide = outshine::Ground::kStructureCellSide;
inline constexpr uint32_t kStructureCellsPerTile = outshine::Ground::kStructureCellsPerTile;
inline constexpr double kStructureLatitudeLimitDeg = 0.5 * kDegPerHalfTurn;

struct StructureCell {
  uint32_t Index = 0;
  outshine::Ground::GeoBounds Footprint;
};

[[nodiscard]] inline std::optional<StructureCell>
StructureCellOf(const outshine::Ground::GeoBounds &tile, std::span<const double> ring) noexcept {
  if (ring.size() < 6 || ring.size() % 2 != 0 || !(tile.MinLonDeg < tile.MaxLonDeg) ||
      !(tile.MinLatDeg < tile.MaxLatDeg)) {
    return std::nullopt;
  }
  const double centreLon = 0.5 * (tile.MinLonDeg + tile.MaxLonDeg);
  outshine::Ground::GeoBounds bounds{.MinLonDeg = kDegPerTurn,
                                     .MinLatDeg = kStructureLatitudeLimitDeg,
                                     .MaxLonDeg = -kDegPerTurn,
                                     .MaxLatDeg = -kStructureLatitudeLimitDeg};
  for (size_t at = 0; at < ring.size(); at += 2) {
    const double lat = ring[at];
    const double lon = ring[at + 1];
    if (!std::isfinite(lat) || !std::isfinite(lon) || std::abs(lat) > kStructureLatitudeLimitDeg ||
        std::abs(lon) > kDegPerHalfTurn) {
      return std::nullopt;
    }
    const double unwrappedLon = centreLon + std::remainder(lon - centreLon, kDegPerTurn);
    bounds.MinLatDeg = std::min(bounds.MinLatDeg, lat);
    bounds.MaxLatDeg = std::max(bounds.MaxLatDeg, lat);
    bounds.MinLonDeg = std::min(bounds.MinLonDeg, unwrappedLon);
    bounds.MaxLonDeg = std::max(bounds.MaxLonDeg, unwrappedLon);
  }
  const double centreLat = 0.5 * (bounds.MinLatDeg + bounds.MaxLatDeg);
  const double centreFootprintLon = 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg);
  const auto column = static_cast<uint32_t>(
      std::clamp(std::floor((centreFootprintLon - tile.MinLonDeg) /
                            (tile.MaxLonDeg - tile.MinLonDeg) * kStructureCellSide),
                 0.0,
                 static_cast<double>(kStructureCellSide - 1)));
  const auto row = static_cast<uint32_t>(
      std::clamp(std::floor((tile.MaxLatDeg - centreLat) / (tile.MaxLatDeg - tile.MinLatDeg) *
                            kStructureCellSide),
                 0.0,
                 static_cast<double>(kStructureCellSide - 1)));
  return StructureCell{.Index = 1 + row * kStructureCellSide + column, .Footprint = bounds};
}

}

#endif
