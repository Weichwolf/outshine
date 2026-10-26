#include "WaterSurfaceTopology.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

WaterAsset Source(float first, float second) {
  return WaterAsset({.Surfaces = {{.FirstRing = 0, .RingCount = 2, .LevelM = first},
                                  {.FirstRing = 2, .RingCount = 2, .LevelM = second}},
                     .Rings = {{.FirstPoint = 0, .PointCount = 4},
                               {.FirstPoint = 4, .PointCount = 4},
                               {.FirstPoint = 8, .PointCount = 4},
                               {.FirstPoint = 4, .PointCount = 4}},
                     .Courses = {},
                     .Levels = {},
                     .Points = {-.001,  -.002,  -.001, .001,   .001,  .001,  .001,   -.002,
                                -.0002, -.0002, .0002, -.0002, .0002, .0002, -.0002, .0002,
                                -.001,  -.001,  -.001, .002,   .001,  .002,  .001,   -.001},
                     .Tiles = {},
                     .ProcessedTiles = 0,
                     .Sources = {},
                     .Inputs = {}});
}

double RingArea(std::span<const LongitudeLatitude> ring, const TangentFrame &frame) {
  double twice = 0;
  for (size_t at = 0; at < ring.size(); ++at) {
    const auto a = frame.ToLocalGroundPosition(ring[at]);
    const auto b = frame.ToLocalGroundPosition(ring[(at + 1) % ring.size()]);
    twice += a.EastM * b.NorthM - a.NorthM * b.EastM;
  }
  return std::abs(twice) / 2;
}

void CheckCoverage(const WaterSurfaceTopology &topology, const TangentFrame &frame) {
  const std::array<LongitudeLatitude, 4> outer{{{.LongitudeDeg = -.002, .LatitudeDeg = -.001},
                                                {.LongitudeDeg = .002, .LatitudeDeg = -.001},
                                                {.LongitudeDeg = .002, .LatitudeDeg = .001},
                                                {.LongitudeDeg = -.002, .LatitudeDeg = .001}}};
  const std::array<LongitudeLatitude, 4> hole{{{.LongitudeDeg = -.0002, .LatitudeDeg = -.0002},
                                               {.LongitudeDeg = .0002, .LatitudeDeg = -.0002},
                                               {.LongitudeDeg = .0002, .LatitudeDeg = .0002},
                                               {.LongitudeDeg = -.0002, .LatitudeDeg = .0002}}};
  const double expected = RingArea(outer, frame) - RingArea(hole, frame);
  double drawn = 0;
  for (const auto &face : topology.Faces) {
    const auto &a = topology.VerticesXY[face.Vertices[0]];
    const auto &b = topology.VerticesXY[face.Vertices[1]];
    const auto &c = topology.VerticesXY[face.Vertices[2]];
    const double twice = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    CHECK(twice > 0, "every physical facet has positive horizontal orientation");
    drawn += twice / 2;
  }
  CHECK(std::abs(topology.AreaM2 - expected) < 1e-7 && std::abs(drawn - expected) < 1e-7,
        "the shared union preserves the outer boundary and island without double coverage");
}

void CheckReplay(const WaterAsset &source, WaterSurfaceTopology topology) {
  std::vector<Vec3> fitted;
  fitted.reserve(topology.VerticesXY.size());
  for (const auto &xy : topology.VerticesXY) {
    fitted.push_back({{xy[0], xy[1], 20 + xy[0] * .001}});
  }
  auto mesh =
      WaterSurfaceMesh::Build(topology.Anchor, std::move(fitted), std::move(topology.Faces));
  CHECK(mesh, "the shared topology accepts one common fitted ENU height per vertex");
  if (!mesh) { return; }
  const auto prepared =
      source.WithSurfaces({.Mesh = std::move(*mesh),
                           .Rejections = {},
                           .FaceSourceOffsets = std::move(topology.FaceSourceOffsets),
                           .FaceSources = std::move(topology.FaceSources),
                           .ProfileDeclines = {}});
  CHECK(prepared, "no admitted source is lost or treated as an omission");
  if (!prepared) { return; }
  const auto encoded = prepared->EncodeNative(1 << 20);
  CHECK(encoded, "the prepared common surface can be saved as a finished asset");
  if (!encoded) { return; }
  const auto restored = WaterAsset::DecodeNative(*encoded);
  CHECK(restored && !restored->LevelAt({}) &&
            restored->LevelAt({.LongitudeDeg = .0015}).has_value(),
        "native replay keeps the hole empty and the nonprimary source extension wet");
  CHECK(restored && restored->EncodeNative(1 << 20) == encoded,
        "native loading preserves the complete shared geometry and source relation");
}

void CheckBudgets(const WaterAsset &source, const TangentFrame &frame) {
  const std::array<uint32_t, 2> selected{0, 1};
  const auto limited = [&](auto change) {
    WaterTopologyBudget budget;
    change(budget);
    const auto result = BuildWaterSurfaceTopology(source, selected, frame, budget);
    CHECK(!result && result.error() == WaterTopologyError::CapacityExceeded,
          "an exhausted local construction budget returns an explicit absent body");
  };
  limited([](auto &b) { b.Sources = 1; });
  limited([](auto &b) { b.SourceCoordinates = 1; });
  limited([](auto &b) { b.Vertices = 1; });
  limited([](auto &b) { b.Faces = 1; });
  limited([](auto &b) { b.Intersections = 0; });
  limited([](auto &b) { b.Memberships = 0; });
  std::stop_source stopped;
  stopped.request_stop();
  const auto canceled = BuildWaterSurfaceTopology(source, selected, frame, {}, stopped.get_token());
  CHECK(!canceled && canceled.error() == WaterTopologyError::Canceled,
        "canceled work publishes no topology");
  const auto spacing = BuildWaterSurfaceTopology(source, selected, frame, {.BoundarySpacingM = 0});
  CHECK(!spacing && spacing.error() == WaterTopologyError::InvalidInput,
        "invalid refinement cannot enter GEOS");
  const std::array<uint32_t, 2> duplicate{0, 0};
  const std::array<uint32_t, 2> unknown{0, 2};
  CHECK(!BuildWaterSurfaceTopology(source, duplicate, frame) &&
            !BuildWaterSurfaceTopology(source, unknown, frame) &&
            !BuildWaterSurfaceTopology(source, {}, frame),
        "only a nonempty unambiguous set of original surface indices is accepted");
  const auto folded =
      BuildWaterSurfaceTopology(source, selected, TangentFrame::At({.LongitudeDeg = 180}));
  CHECK(!folded && folded.error() == WaterTopologyError::InvalidGeometry,
        "an opposite-hemisphere polygon cannot fold onto the local horizontal domain");
}
}

int main() {
  const auto source = Source(100, 1500);
  const auto frame = TangentFrame::At({});
  const std::array<uint32_t, 2> selected{0, 1};
  const std::array<uint32_t, 2> reversed{1, 0};
  const auto topology = BuildWaterSurfaceTopology(source, selected, frame);
  CHECK(topology, "overlapping source fragments produce a shared horizontal domain");
  if (!topology) { return Report(); }
  CheckCoverage(*topology, frame);
  CHECK(topology->FaceSourceOffsets.size() == topology->Faces.size() + 1 &&
            topology->FaceSources.size() > topology->Faces.size(),
        "positive overlaps preserve multiple original sources on a single facet");
  const auto changed = BuildWaterSurfaceTopology(Source(-500, 9), reversed, frame);
  CHECK(changed && changed->VerticesXY == topology->VerticesXY &&
            changed->FaceSources == topology->FaceSources &&
            changed->FaceSourceOffsets == topology->FaceSourceOffsets,
        "source order and arbitrary height priors cannot split the common XY geometry");
  CheckReplay(source, *topology);
  CheckBudgets(source, frame);
  return Report();
}
