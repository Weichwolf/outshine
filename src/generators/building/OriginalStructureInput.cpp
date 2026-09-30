#include "OriginalStructureInput.h"

#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace outshine::Generators {
namespace {

std::expected<GeographicRing, OriginalStructureInputError>
PointRing(RawTile &raw, uint32_t point, double widthM) {
  if (!std::isfinite(widthM) || widthM <= 0.0) {
    return std::unexpected(OriginalStructureInputError::InvalidPointPolicy);
  }
  const auto frame = TangentFrame::At(
      {.LongitudeDeg = raw.LatLon[2 * point + 1], .LatitudeDeg = raw.LatLon[2 * point]});
  const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
  const double half = widthM / 2.0;
  for (const auto corner :
       std::array<EastNorth, 4>{{{-half, -half}, {half, -half}, {half, half}, {-half, half}}}) {
    const auto at = frame.ApproximateGeographicAt(corner);
    raw.LatLon.push_back(at.LatitudeDeg);
    raw.LatLon.push_back(std::remainder(at.LongitudeDeg, 360.0));
  }
  return GeographicRing{.First = first, .Count = 4, .Exterior = true};
}

}

std::expected<RawTile, OriginalStructureInputError>
OriginalStructureInput(const outshine::Ground::OsmBuildingFootprints &buildings,
                       StructureOriginalSource source,
                       OriginalStructurePolicy policy) {
  if (source.Snapshot.get() != &buildings.Source()) {
    return std::unexpected(OriginalStructureInputError::SourceMismatch);
  }
  policy.PointsMost =
      std::min(policy.PointsMost, static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  if (buildings.Points().size() / 2 > policy.PointsMost) {
    return std::unexpected(OriginalStructureInputError::PointBudgetExceeded);
  }
  RawTile raw;
  raw.Original = std::move(source);
  raw.LatLon.assign(buildings.Points().begin(), buildings.Points().end());
  raw.Holes.assign(buildings.Rings().begin(), buildings.Rings().end());
  raw.Structures.reserve(buildings.Buildings().size());
  const auto &coverage = raw.Original.Bounds;
  const outshine::Ground::GeoBounds bounds{.MinLonDeg = coverage.WestDeg,
                                           .MinLatDeg = coverage.SouthDeg,
                                           .MaxLonDeg = coverage.EastDeg,
                                           .MaxLatDeg = coverage.NorthDeg};
  for (const auto &building : buildings.Buildings()) {
    const auto height = buildings.Heights(building).Resolve(policy.Heights);
    if (!height) { return std::unexpected(OriginalStructureInputError::InvalidHeight); }
    GeographicRing ring;
    uint32_t firstHole = 0;
    uint32_t holeCount = 0;
    if (building.PointIndex) {
      if (policy.PointsMost - raw.LatLon.size() / 2 < 4) {
        return std::unexpected(OriginalStructureInputError::PointBudgetExceeded);
      }
      const auto generated = PointRing(raw, *building.PointIndex, policy.PointWidthM);
      if (!generated) { return std::unexpected(generated.error()); }
      ring = *generated;
    } else {
      const auto rings = buildings.Rings().subspan(building.FirstRing, building.RingCount);
      if (rings.empty() || !rings.front().Exterior) {
        return std::unexpected(OriginalStructureInputError::UnassignedCourtyard);
      }
      ring = rings.front();
      for (const auto &hole : rings.subspan(1)) {
        if (hole.Exterior) {
          return std::unexpected(OriginalStructureInputError::UnassignedCourtyard);
        }
      }
      firstHole = static_cast<uint32_t>(building.FirstRing + 1);
      holeCount = static_cast<uint32_t>(building.RingCount - 1);
    }
    const auto points = std::span<const double>(raw.LatLon).subspan(2 * ring.First, 2 * ring.Count);
    const auto cell = StructureCellOf(bounds, points);
    if (!cell) { return std::unexpected(OriginalStructureInputError::InvalidCell); }
    raw.Structures.push_back({.LocalFirst = ring.First,
                              .PointCount = ring.Count,
                              .SourceFirst = ring.First,
                              .FirstHole = firstHole,
                              .HoleCount = holeCount,
                              .SourceFirstHole = firstHole,
                              .Cell = *cell,
                              .HeightM = height->TopM,
                              .MinimumHeightM = height->MinimumM,
                              .HeightOrigin = height->TopOrigin,
                              .OriginalId = building.Source});
  }
  return raw;
}

}
