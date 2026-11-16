#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "PreparedStructureTile.h"
#include "StructureSourceErrorTask.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr size_t kBuildings = 100;
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 0}, raw.AnchorEcef);
  for (size_t i = 0; i < kBuildings; ++i) {
    const double lat = 47 + static_cast<double>(i / 10) * 0.00015;
    const double lon = 9 + static_cast<double>(i % 10) * 0.0002;
    const size_t first = raw.LatLon.size() / 2;
    raw.LatLon.insert(
        raw.LatLon.end(),
        {lat, lon, lat, lon + 0.0001, lat + 0.00008, lon + 0.0001, lat + 0.00008, lon});
    raw.Structures.push_back({.LocalFirst = static_cast<uint32_t>(first),
                              .PointCount = 4,
                              .SourceFirst = static_cast<uint32_t>(first),
                              .Cell = {.Index = 1,
                                       .Footprint = {.MinLonDeg = lon,
                                                     .MinLatDeg = lat,
                                                     .MaxLonDeg = lon + 0.0001,
                                                     .MaxLatDeg = lat + 0.00008}},
                              .HeightM = 10,
                              .WallColour = {},
                              .Facade = {}});
  }
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights, "a physical terrain field covers every declared building");
  if (!heights) { return Report(); }
  const auto base = PrepareStructureTile(raw, *heights);
  CHECK(base && base->Structures.size() == kBuildings,
        "the native prepared tile retains every separated source building");
  if (!base) { return Report(); }
  RawTile view;
  view.RequestedCell = 1;
  view.RequestedDetail = LevelOfDetail::Shell;
  constexpr uint64_t kSource = 91;
  StructureSourceErrorTask task;
  CHECK(task.Reset(*base, view, kSource, {.TargetUncertaintyM = 0.5}),
        "a source-matched native comparison starts for the complete tile");
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  const std::atomic_bool stopped{false};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  bool complete = false;
  while (!complete && std::chrono::steady_clock::now() < deadline) {
    const auto advanced = task.Step(mesher, *scratch, stopped);
    CHECK(advanced, "every source pair retains valid native geometry");
    if (!advanced) { return Report(); }
    complete = *advanced == StructureSurfaceErrorProgress::Complete;
  }
  const auto bound = task.Bound(kSource);
  CHECK(complete && bound && std::isfinite(bound->UpperDistanceM()) &&
            bound->UpperDistanceM() <= 0.5 && !task.Bound(kSource + 1),
        "a hundred separated buildings receive a useful source-bound whole-tile error");
  return Report();
}
