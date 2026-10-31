#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "PreparedStructurePlan.h"
#include "PreparedStructureTile.h"
#include "StructureBake.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <optional>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.TileSpanM = 1000;
  raw.Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  const double metresPerLon = kMPerDegLon * std::cos(47 * kDeg2Rad);
  const std::array ring{47.0,
                        9.0,
                        47.0,
                        9 + 12 / metresPerLon,
                        47 + 16 / kMPerDegLat,
                        9 + 12 / metresPerLon,
                        47 + 16 / kMPerDegLat,
                        9.0};
  const std::array tiny{47 + 1 / kMPerDegLat,
                        9 + 1 / metresPerLon,
                        47 + 1 / kMPerDegLat,
                        9 + 1.5 / metresPerLon,
                        47 + 1.5 / kMPerDegLat,
                        9 + 1 / metresPerLon};
  for (const auto points : {std::span<const double>(ring), std::span<const double>(tiny)}) {
    const auto cell = StructureCellOf(bounds, points);
    CHECK(cell, "each footprint has a valid spatial cell");
    if (!cell) { return Report(); }
    raw.Structures.push_back({.LocalFirst = static_cast<uint32_t>(raw.LatLon.size() / 2),
                              .PointCount = static_cast<uint32_t>(points.size() / 2),
                              .Cell = *cell,
                              .HeightM = 9,
                              .Pitched = 0,
                              .WallColour = {},
                              .Facade = {}});
    raw.LatLon.insert(raw.LatLon.end(), points.begin(), points.end());
  }
  CHECK(raw.Structures[0].Cell.Index == raw.Structures[1].Cell.Index,
        "the normal and rejected footprints share one spatial cell");
  const size_t cellAt = raw.Structures[0].Cell.Index - 1u;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  auto prepared = PrepareStructureTile(raw, *heights);
  CHECK(prepared && prepared->Structures.size() == 2 && prepared->Surfaces.size() == 2,
        "prepared input retains both physical source footprints");
  if (!prepared) { return Report(); }
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  auto omitted = PreparedStructurePlan(prepared->Structures[1],
                                       prepared->PointsLatLon,
                                       prepared->Holes,
                                       std::span(prepared->CornerAslM).subspan(4, 3),
                                       prepared->AnchorEcef);
  const auto unprepared = mesher.KnownEmptySurface(omitted);
  CHECK(unprepared && !*unprepared, "unprepared data is not classified as known empty");
  omitted.Prepared = &prepared->Surfaces[1];
  const auto empty = mesher.KnownEmptySurface(omitted);
  CHECK(empty && *empty, "the prepared native recipe explicitly has no surface");
  omitted.HeightM = std::numeric_limits<double>::quiet_NaN();
  const auto invalid = mesher.KnownEmptySurface(omitted);
  CHECK(invalid && !*invalid, "empty metadata cannot hide invalid plan parameters");
  auto control = *prepared;
  control.Structures.resize(1);
  control.Surfaces.resize(1);
  const std::array<std::optional<LevelOfDetail>, 4> details{
      std::nullopt, LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed};
  for (const auto detail : details) {
    raw.RequestedDetail = detail;
    BakedTile actual;
    BakedTile expected;
    auto all = BakePreparedStructures(*prepared, raw, mesher, *scratch);
    auto one = BakePreparedStructures(control, raw, mesher, *scratch);
    CHECK(all && one, "full and renderable-only prepared sources bake at every detail");
    if (!all || !one) { continue; }
    actual = std::move(*all);
    expected = std::move(*one);
    CHECK(actual.Prints.size() == 2 && actual.UnsupportedMeshes == 1,
          "the source and its explicit omission remain counted");
    CHECK(actual.Digest == expected.Digest && actual.OccupiedCells == expected.OccupiedCells,
          "known-empty input adds no Fine, Shell, Massed or projected geometry");
    CHECK(actual.CellShellErrorM[cellAt] == expected.CellShellErrorM[cellAt] &&
              actual.CellShellErrorM[cellAt] > 0.16,
          "the unchanged nonempty producer bound survives its empty neighbour");
  }
  return Report();
}
