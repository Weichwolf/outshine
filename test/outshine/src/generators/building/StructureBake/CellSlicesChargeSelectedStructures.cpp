#include "PreparedStructureTile.h"
#include "StructureBake.h"
#include "BuildingMesh.h"
#include "Check.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kSourceCount = 1024;
constexpr double kSpanM = 2000;

RawTile Inputs() {
  RawTile raw;
  raw.TileSpanM = kSpanM;
  constexpr double kLon = 9;
  constexpr double kLat = 47;
  constexpr double kStep = 0.008;
  constexpr double kSize = 0.0001;
  const Ground::GeoBounds region{.MinLonDeg = kLon,
                                 .MinLatDeg = kLat,
                                 .MaxLonDeg = kLon + 2 * kStep,
                                 .MaxLatDeg = kLat + 2 * kStep};
  for (uint32_t at = 0; at < 2; ++at) {
    const double lon = kLon + at * kStep;
    const double lat = kLat + at * kStep;
    const std::array ring{lat, lon, lat, lon + kSize, lat + kSize, lon + kSize, lat + kSize, lon};
    const auto cell = StructureCellOf(region, ring);
    CHECK(cell, "source footprints have distinct spatial cells");
    if (!cell) { return {}; }
    raw.LatLon.insert_range(raw.LatLon.end(), ring);
    raw.Structures.push_back({.LocalFirst = at * 4,
                              .PointCount = 4,
                              .SourceFirst = at * 4,
                              .Cell = *cell,
                              .HeightM = 12,
                              .Pitched = 0,
                              .WallColour = std::nullopt,
                              .Facade = std::nullopt});
  }
  raw.RequestedCell = raw.Structures.front().Cell.Index;
  raw.RequestedDetail = LevelOfDetail::Shell;
  return raw;
}

void CheckSlices(const PreparedStructureTile &original,
                 const RawTile &input,
                 const Ground::HeightField &heights,
                 const BuildingMesh &mesher,
                 bool atEnd,
                 bool prepared) {
  auto raw = input;
  auto base = original;
  const size_t selected = atEnd ? kSourceCount - 1 : 0;
  raw.Structures.assign(kSourceCount, input.Structures.back());
  raw.Structures[selected] = input.Structures.front();
  base.Structures.assign(kSourceCount, original.Structures.back());
  base.Surfaces.assign(kSourceCount, original.Surfaces.back());
  base.Structures[selected] = original.Structures.front();
  base.Surfaces[selected] = original.Surfaces.front();
  auto scratch = mesher.Scratch();
  const auto reference = BakePreparedStructures(original, input, mesher, *scratch);
  CHECK(reference && reference->Prints.size() == 1, "one cell supplies the geometry reference");
  if (!reference) { return; }
  StructureBakeProgress progress;
  bool complete = false;
  size_t calls = 0;
  while (!complete && calls < 4) {
    const auto advanced = prepared ? progress.AdvancePrepared(base, raw, mesher, *scratch, 1)
                                   : progress.AdvanceStructures(raw, heights, mesher, *scratch, 1);
    CHECK(advanced, "a bounded sparse-cell slice succeeds");
    if (!advanced) { return; }
    complete = *advanced;
    ++calls;
  }
  CHECK(complete && calls <= 2,
        "foreign cells do not consume the selected-source quota or create empty worker slices");
  if (!complete) { return; }
  const auto tile = progress.Finalize(raw, mesher, *scratch);
  CHECK(tile && tile->Digest == reference->Digest && tile->Prints == reference->Prints &&
            tile->OccupiedCells == reference->OccupiedCells,
        "skipping scheduling gaps preserves exact geometry and complete source contacts");
  auto all = raw;
  all.RequestedCell.reset();
  StructureBakeProgress bounded;
  const auto first = prepared ? bounded.AdvancePrepared(base, all, mesher, *scratch, 1)
                              : bounded.AdvanceStructures(all, heights, mesher, *scratch, 1);
  CHECK(first && !*first && bounded.BakedStructures() == 1,
        "an unfiltered source still yields after exactly one processed structure");
  const std::atomic_bool stopping(true);
  const auto cancelled =
      prepared ? bounded.AdvancePrepared(base, all, mesher, *scratch, 1, &stopping)
               : bounded.AdvanceStructures(all, heights, mesher, *scratch, 1, &stopping);
  CHECK(!cancelled, "cancellation remains observable before the next planning slice");
}
}

int main() {
  const auto raw = Inputs();
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  const auto base = PrepareStructureTile(raw, *heights);
  CHECK(base && base->Structures.size() == 2, "the shared source is enriched once");
  if (!base) { return Report(); }
  const BuildingMesh mesher;
  for (const bool prepared : {false, true}) {
    for (const bool atEnd : {false, true}) {
      CheckSlices(*base, raw, *heights, mesher, atEnd, prepared);
    }
  }
  return Report();
}
