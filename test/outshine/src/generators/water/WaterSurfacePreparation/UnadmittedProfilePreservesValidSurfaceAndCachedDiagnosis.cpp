#include "WaterSurfacePreparation.h"
#include "WaterSurfaceBuilder.h"
#include "WaterSurfaceTopology.h"
#include "Check.h"
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;

WaterAsset Source() {
  const uint32_t count = WaterTopologyBudget{}.Vertices + 1;
  WaterAsset::Data data{
      .Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 100, .Input = 0}},
      .Rings = {{.FirstPoint = 0, .PointCount = count}},
      .Courses =
          {{.FirstPoint = count, .PointCount = 2, .FirstLevel = 0, .HalfWidthM = 1, .Input = 1}},
      .Levels = {100, 99},
      .Points = {},
      .Tiles = {{.FirstSurface = 0, .SurfaceCount = 1}},
      .ProcessedTiles = 1,
      .Sources = {{.Identity = {.Kind = Data::DataKind::VectorMap,
                                .SourceId = "fixture",
                                .Revision = "r1"},
                   .InputDigest = {},
                   .FeatureCount = 2}},
      .Inputs = {{.Source = 0,
                  .Feature = 0,
                  .FeatureId = {},
                  .Layer = "water_polygons",
                  .Kind = "river",
                  .Crossing = {},
                  .Intermittent = {}},
                 {.Source = 0,
                  .Feature = 1,
                  .FeatureId = {},
                  .Layer = "water_lines",
                  .Kind = "river",
                  .Crossing = {},
                  .Intermittent = {}}}};
  for (uint32_t at = 0; at < count; ++at) {
    const double angle = 2 * std::numbers::pi * at / count;
    data.Points.insert(data.Points.end(), {.0001 * std::sin(angle), .0001 * std::cos(angle)});
  }
  data.Points.insert(data.Points.end(), {0, -.00005, 0, .00005});
  return WaterAsset(std::move(data));
}
}

int main() {
  const auto source = Source();
  const auto frame = TangentFrame::At({});
  const auto prepared = PrepareWaterSurfaces(source, frame);
  CHECK(prepared && prepared->SurfacesReady() && prepared->SharesInputsWith(source),
        "a bounded shared model must not erase an independently valid source surface");
  if (!prepared) { return outshine::Test::Report(); }
  const auto *geometry = prepared->PreparedSurfaces();
  CHECK(geometry && geometry->Rejections.empty() && !geometry->Mesh.Faces().empty(),
        "only invalid individual surfaces are recorded as omitted");
  CHECK(geometry && geometry->ProfileDeclines.size() == 1 &&
            geometry->ProfileDeclines[0].Surface == 0 &&
            geometry->ProfileDeclines[0].Reason ==
                WaterAsset::SurfaceRejectionReason::CapacityExceeded,
        "the unavailable coupled model remains explicit with source and cause");
  const auto bytes = prepared->EncodeNative(16 << 20);
  CHECK(bytes, "the valid surface and its declined profile encode together");
  if (!bytes) { return outshine::Test::Report(); }
  const auto replay = WaterAsset::DecodeNative(*bytes);
  CHECK(replay && replay->PreparedSurfaces()->ProfileDeclines.size() == 1,
        "offline replay preserves the model diagnosis");
  if (replay) {
    const auto metrics = InspectWaterSurfaceGeometry(*replay);
    CHECK(metrics.ProfileDeclines == 1 && metrics.RefusedCapacity == 0 &&
              metrics.RefusedTopology == 0 && metrics.Laid == 1 && metrics.Triangles > 0,
          "cached state distinguishes a declined profile from a missing surface");
    const auto height = replay->LevelAt({});
    CHECK(height && std::abs(*height - 100) < 1e-4,
          "the retained individual surface is available to runtime height consumers");
    const auto reused = PrepareWaterSurfaces(*replay, frame);
    CHECK(reused && reused->PreparedSurfaces() == replay->PreparedSurfaces(),
          "cache hits reuse the finished product without retrying its declined profile");
  }
  return outshine::Test::Report();
}
