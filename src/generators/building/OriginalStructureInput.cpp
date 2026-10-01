#include "OriginalStructureInput.h"

#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Generators {
namespace {

constexpr double kLongitudePeriodDeg = 360.0;

std::expected<GeographicRing, OriginalStructureInputError>
PointRing(RawTile &raw, const OriginalStructurePolicy &policy, uint32_t point) {
  const double widthM = policy.PointWidthM;
  if (!std::isfinite(widthM) || widthM <= 0.0) {
    return std::unexpected(OriginalStructureInputError::InvalidPointPolicy);
  }
  const size_t pointAt = static_cast<size_t>(point) * 2;
  const auto frame = TangentFrame::At(
      {.LongitudeDeg = raw.LatLon[pointAt + 1], .LatitudeDeg = raw.LatLon[pointAt]});
  const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
  const double half = widthM / 2.0;
  for (const auto corner : std::array<EastNorth, 4>{{{.EastM = -half, .NorthM = -half},
                                                     {.EastM = half, .NorthM = -half},
                                                     {.EastM = half, .NorthM = half},
                                                     {.EastM = -half, .NorthM = half}}}) {
    const auto at = frame.ApproximateGeographicAt(corner);
    raw.LatLon.push_back(at.LatitudeDeg);
    raw.LatLon.push_back(std::remainder(at.LongitudeDeg, kLongitudePeriodDeg));
  }
  return GeographicRing{.First = first, .Count = 4, .Exterior = true};
}

}

std::expected<RawTile, OriginalStructureInputError>
OriginalStructureInput(const outshine::Ground::OsmBuildingFootprints &buildings,
                       OriginalStructureSource source,
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
  const auto &coverage = raw.Original.Origin.Bounds;
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
      const auto generated = PointRing(raw, policy, *building.PointIndex);
      if (!generated) { return std::unexpected(generated.error()); }
      ring = *generated;
    } else {
      const auto rings = buildings.Rings().subspan(building.FirstRing, building.RingCount);
      if (rings.empty() || !rings.front().Exterior ||
          std::ranges::any_of(rings.subspan(1), [](const auto &hole) { return hole.Exterior; })) {
        return std::unexpected(OriginalStructureInputError::UnassignedCourtyard);
      }
      ring = rings.front();
      firstHole = static_cast<uint32_t>(building.FirstRing + 1);
      holeCount = static_cast<uint32_t>(building.RingCount - 1);
    }
    const auto points =
        std::span<const double>(raw.LatLon)
            .subspan(static_cast<size_t>(ring.First) * 2, static_cast<size_t>(ring.Count) * 2);
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
  std::vector<Data::OsmElementId> roots;
  roots.reserve(raw.Structures.size());
  for (const auto &structure : raw.Structures) { roots.push_back(structure.OriginalId); }
  auto selected = raw.Original.Snapshot->Elements.SelectReferenced(roots);
  if (!selected) { return std::unexpected(OriginalStructureInputError::MissingReference); }
  raw.Original.Origin.Provenance = DescribeOsmSource(*raw.Original.Snapshot);
  raw.Original.Archive = raw.Original.Snapshot;
  const auto &archive = *raw.Original.Snapshot;
  raw.Original.Snapshot = std::make_shared<const Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*selected),
                              .Coverage = archive.Coverage,
                              .SourceBytes = archive.SourceBytes,
                              .ReadMs = archive.ReadMs,
                              .ParseMs = archive.ParseMs,
                              .Chunks = archive.Chunks,
                              .Cell = archive.Cell});
  return raw;
}

}
