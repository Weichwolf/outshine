#include "RiverBodyHeight.h"
#include "WaterSurfacePreparation.h"
#include "WaterPhysicalFrame.h"
#include "Check.h"
#include <cmath>
#include <stop_token>
#include <utility>
#include <expected>
#include <string>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Generators;

WaterAsset Source(float lastLevel = 99) {
  WaterAsset::Data data{
      .Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 80, .Input = 0},
                   {.FirstRing = 1, .RingCount = 1, .LevelM = 120, .Input = 1}},
      .Rings = {{.FirstPoint = 0, .PointCount = 4}, {.FirstPoint = 4, .PointCount = 4}},
      .Courses = {{.FirstPoint = 8, .PointCount = 2, .FirstLevel = 0, .HalfWidthM = 1, .Input = 2}},
      .Levels = {100, lastLevel},
      .Points = {0, 0,     0,     .0001, .0001, .0001, .0001,  0, 0,      .0001,
                 0, .0002, .0001, .0002, .0001, .0001, .00005, 0, .00005, .0002},
      .Tiles = {{.FirstSurface = 0, .SurfaceCount = 2}},
      .ProcessedTiles = 1,
      .Sources = {{.Identity = {.Kind = Data::DataKind::VectorMap,
                                .SourceId = "fixture",
                                .Revision = "r1"},
                   .InputDigest = {},
                   .FeatureCount = 3}},
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
                  .Layer = "water_polygons",
                  .Kind = "river",
                  .Crossing = {},
                  .Intermittent = {}},
                 {.Source = 0,
                  .Feature = 2,
                  .FeatureId = {},
                  .Layer = "water_lines",
                  .Kind = "river",
                  .Crossing = {},
                  .Intermittent = {}}}};
  return WaterAsset(std::move(data));
}

void CheckBody(float endLevel) {
  const auto source = Source(endLevel);
  const auto frame = TangentFrame::At({});
  const auto plan = PlanRiverBodies(source, frame, WaterGroundPolicy::AssumeUnlabelledGround);
  CHECK(plan && plan->Bodies.size() == 1,
        "connected river fragments have one shared height domain");
  if (!plan || plan->Bodies.size() != 1) { return; }
  const auto &body = plan->Bodies.front();
  auto fit = FitRiverBody(source, frame, body);
  CHECK(fit && fit->WholeFaceViolation <= 1e-9 && fit->MaximumTargetErrorM < 1e-3,
        "the physical fit follows course targets with unchanged whole-facet conditions");
  if (!fit) { return; }
  const auto mesh =
      WaterSurfaceMesh::Build(frame.Anchor(), std::move(fit->VerticesEnuM), body.Surface.Faces);
  CHECK(mesh, "the fitted body is directly renderable and queryable");
  if (!mesh) { return; }
  for (const double longitude : {.000025, .000075, .0001, .000125, .000175}) {
    const auto height = mesh->LevelAt({.LongitudeDeg = longitude, .LatitudeDeg = .00005});
    const double expected = 100 + (endLevel - 100) * longitude / .0002;
    CHECK(height && std::abs(*height - expected) < 1e-3,
          "height queries cross the original 80m/120m fragment seam continuously");
  }
  const auto prepared = PrepareWaterSurfaces(source, frame);
  CHECK(prepared && prepared->SurfacesReady() && prepared->SharesInputsWith(source) &&
            prepared->PreparedSurfaces()->Rejections.empty(),
        "the production cooker publishes fitted geometry without rewriting original inputs");
  if (prepared) {
    const auto encoded = prepared->EncodeNative(1 << 20);
    CHECK(encoded, "complete source metadata and fitted geometry encode together");
    const auto replay =
        encoded ? WaterAsset::DecodeNative(*encoded)
                : std::expected<WaterAsset, std::string>(std::unexpected("encode failed"));
    CHECK(replay && replay->SurfacesReady(),
          "finished fitted geometry survives native asset replay");
    const LongitudeLatitude query{.LongitudeDeg = .0001, .LatitudeDeg = .00005};
    const auto height = replay ? replay->LevelAt(query) : std::nullopt;
    CHECK(height && std::abs(*height - (100 + endLevel) / 2) < 1e-3,
          "runtime and cached height consumers use the fitted body across the old fragment seam");
  }
  RiverHeightBudget bounded;
  bounded.Fit.Rows = 1;
  const auto refused = FitRiverBody(source, frame, body, bounded);
  CHECK(!refused && refused.error() == WaterHeightError::CapacityExceeded,
        "insufficient row budget rejects the fit before unbounded assembly");
  std::stop_source canceled;
  canceled.request_stop();
  const auto stopped = FitRiverBody(source, frame, body, {}, canceled.get_token());
  CHECK(!stopped && stopped.error() == WaterHeightError::Canceled,
        "canceled height work cannot publish a partial body");
}
}

int main() {
  CheckBody(99);
  CheckBody(100);
  return outshine::Test::Report();
}
