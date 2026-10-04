#include "StructureInput.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Generators {

std::expected<RawTile, StructureInputError>
StructureInput(outshine::Ground::StructureFootprints footprints) {
  RawTile raw;
  raw.LatLon = std::move(footprints.LatLon);
  raw.Holes = std::move(footprints.Rings);
  raw.SourceInputs.Origin = std::move(footprints.Origin);
  raw.Structures.reserve(footprints.Structures.size());
  const auto &coverage = raw.SourceInputs.Origin.Bounds;
  const outshine::Ground::GeoBounds bounds{.MinLonDeg = coverage.WestDeg,
                                           .MinLatDeg = coverage.SouthDeg,
                                           .MaxLonDeg = coverage.EastDeg,
                                           .MaxLatDeg = coverage.NorthDeg};
  for (const auto &footprint : footprints.Structures) {
    const size_t first = static_cast<size_t>(footprint.First) * 2;
    const size_t count = static_cast<size_t>(footprint.Count) * 2;
    if (first > raw.LatLon.size() || count > raw.LatLon.size() - first) {
      return std::unexpected(StructureInputError::InvalidCell);
    }
    const auto cell = StructureCellOf(bounds, std::span(raw.LatLon).subspan(first, count));
    if (!cell) { return std::unexpected(StructureInputError::InvalidCell); }
    raw.Structures.push_back({.LocalFirst = footprint.First,
                              .PointCount = footprint.Count,
                              .SourceFirst = footprint.First,
                              .FirstHole = footprint.FirstHole,
                              .HoleCount = footprint.HoleCount,
                              .SourceFirstHole = footprint.FirstHole,
                              .Cell = *cell,
                              .HeightM = footprint.Height.TopM,
                              .MinimumHeightM = footprint.Height.MinimumM,
                              .WallColour = {},
                              .HeightOrigin = footprint.Height.TopOrigin,
                              .SourceId = footprint.Source});
  }
  return raw;
}

}
