#include "WaterSurfaceBuilder.h"
#include "WaterAsset.h"
#include "WaterSurfaceMesh.h"
#include "TangentFrame.h"
#include "Check.h"
#include "scene/Geometry.h"
#include <cmath>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const WaterAsset source({.Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 999},
                                        {.FirstRing = 0, .RingCount = 1, .LevelM = 888}},
                           .Rings = {{.FirstPoint = 0, .PointCount = 3}},
                           .Points = {-.001, -.001, -.001, .001, .001, 0},
                           .Tiles = {{.Zoom = 0, .X = 0, .Y = 0, .SurfaceCount = 2}},
                           .ProcessedTiles = 1});
  auto mesh = WaterSurfaceMesh::Build(
      {}, {{{-100, -100, 30}}, {{100, -100, 34}}, {{0, 100, 32}}}, {{.Vertices = {0, 1, 2}}});
  CHECK(mesh, "fitted physical facet exists");
  if (!mesh) { return Report(); }
  const auto prepared = source.WithSurfaces(
      {.Mesh = std::move(*mesh), .FaceSourceOffsets = {0, 2}, .FaceSources = {0, 1}});
  CHECK(prepared, "one fitted physical facet preserves both overlapping source surfaces");
  if (!prepared) { return Report(); }
  const auto encoded = prepared->EncodeNative(16384);
  CHECK(encoded, "shared fitted water is cached without repeating the solver");
  if (!encoded) { return Report(); }
  const auto water = WaterAsset::DecodeNative(*encoded);
  CHECK(water, "render and contact consumers load the same shared cached facet");
  if (!water) { return Report(); }
  Geometry geometry;
  const auto material = geometry.addSurface("water", WaterSurfaceMaterial());
  CHECK(material, "water material exists");
  if (!material) { return Report(); }
  const auto frame = TangentFrame::At({});
  const auto drawn = AppendWaterSurfaceGeometry(geometry, *material, *water, frame);
  CHECK(drawn && drawn->Triangles == 1 && water->LevelAt({}) == 32,
        "rendering and contact ignore the obsolete flat prior and use the same fitted facet");
  const auto normals = geometry.normalsOf(0);
  const double length = std::hypot(.02, 1.);
  for (size_t i = 0; i < normals.size(); i += 3) {
    CHECK(std::abs(normals[i] + .02 / length) < 1e-7 &&
              std::abs(normals[i + 1] - 1. / length) < 1e-7 && normals[i + 2] == 0,
          "render normals follow the actual fitted slope rather than a hardcoded up vector");
  }
  const auto rebased = TangentFrame::At({.LongitudeDeg = .05, .LatitudeDeg = .05});
  const auto up = WaterSurfaceUpAt(*water, rebased, {});
  const auto expected = rebased.ToLocalPosition(
      LongitudeLatitudeHeight{.LongitudeDeg = 0, .LatitudeDeg = 0, .HeightM = 32});
  CHECK(up && std::abs(*up - expected.UpM) < 1e-9,
        "rebased terrain and bridge queries preserve the same physical water contact");
  const EastNorthUp local{.EastM = 125, .NorthM = -87, .UpM = 32};
  const auto roundTrip = rebased.ToLocalPosition(rebased.ToEcefPosition(local));
  CHECK(std::abs(roundTrip.EastM - local.EastM) < 1e-8 &&
            std::abs(roundTrip.NorthM - local.NorthM) < 1e-8 &&
            std::abs(roundTrip.UpM - local.UpM) < 1e-8,
        "canonical positions survive the inverse WGS84 tangent transformation");
  return Report();
}
