#include "WaterSurfacePreparation.h"
#include "WaterSurfaceBuilder.h"
#include "WaterAsset.h"
#include "TangentFrame.h"
#include "Check.h"
#include "Geometry.h"
#include <cmath>
#include <stop_token>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const WaterAsset source(
      {.Surfaces = {{.FirstRing = 0, .RingCount = 2, .LevelM = 123}},
       .Rings = {{.FirstPoint = 0, .PointCount = 4}, {.FirstPoint = 4, .PointCount = 4}},
       .Courses = {},
       .Levels = {},
       .Points = {-.001,
                  -.001,
                  -.001,
                  .001,
                  .001,
                  .001,
                  .001,
                  -.001,
                  -.0002,
                  -.0002,
                  .0002,
                  -.0002,
                  .0002,
                  .0002,
                  -.0002,
                  .0002},
       .Tiles = {},
       .Sources = {},
       .Inputs = {}});
  const auto frame = TangentFrame::At({});
  Geometry geometry;
  const auto material = geometry.addSurface("water", WaterSurfaceMaterial());
  CHECK(material, "water material exists");
  if (!material) { return Report(); }
  CHECK(!AppendWaterSurfaceGeometry(geometry, *material, source, frame) && geometry.parts() == 0,
        "render assembly never triangulates unprepared source water");
  const auto prepared = PrepareWaterSurfaces(source, frame);
  CHECK(prepared && prepared->SurfacesReady() && prepared->SharesInputsWith(source),
        "preparation retains immutable input storage and supplies ready geometry");
  if (!prepared) { return Report(); }
  CHECK(!prepared->LevelAt({}) && prepared->LevelAt({.LongitudeDeg = .0005}),
        "canonical triangles retain the island and surrounding wet area");
  const auto drawn = AppendWaterSurfaceGeometry(geometry, *material, *prepared, frame);
  CHECK(drawn && drawn->Laid == 1 && drawn->Triangles == 8 && drawn->RefusedTopology == 0 &&
            drawn->RefusedCapacity == 0 && geometry.parts() == 1,
        "render geometry loads the eight prepared facets around the island");
  const auto vertices = prepared->PreparedSurfaces()->Mesh.VerticesEnuM();
  const auto positions = geometry.positionsOf(0);
  CHECK(positions.size() == vertices.size() * 3, "render storage uses the canonical vertices");
  for (size_t i = 0; i < vertices.size(); ++i) {
    CHECK(positions[i * 3] == static_cast<float>(vertices[i][0]) &&
              positions[i * 3 + 1] == static_cast<float>(vertices[i][2]) &&
              positions[i * 3 + 2] == -static_cast<float>(vertices[i][1]),
          "the actual render buffer is the documented Float32 conversion of canonical geometry");
  }
  const auto bytes = prepared->EncodeNative(16384);
  const auto loaded = bytes ? WaterAsset::DecodeNative(*bytes)
                            : std::expected<WaterAsset, std::string>(std::unexpected("encode"));
  CHECK(loaded && loaded->SurfacesReady(), "native replay supplies ready surface geometry");
  if (loaded) {
    const auto reused = PrepareWaterSurfaces(*loaded, TangentFrame::At({.LongitudeDeg = .1}));
    CHECK(reused && reused->PreparedSurfaces() == loaded->PreparedSurfaces(),
          "a loaded ready asset returns before triangulation or index building, even after rebase");
    const auto rebasedFrame = TangentFrame::At({.LongitudeDeg = .001});
    Geometry rebased;
    const auto surface = rebased.addSurface("water", WaterSurfaceMaterial());
    CHECK(surface && AppendWaterSurfaceGeometry(rebased, *surface, *loaded, rebasedFrame),
          "cached geometry can be placed in a different local frame");
  }
  std::stop_source stop;
  stop.request_stop();
  CHECK(!PrepareWaterSurfaces(source, frame, stop.get_token()),
        "canceled work never begins polygon triangulation");
  return Report();
}
