#include "WaterSurfacePreparation.h"
#include "WaterSurfaceBuilder.h"
#include "WaterAsset.h"
#include "TangentFrame.h"
#include "Check.h"
#include "Geometry.h"

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const WaterAsset source(
      {.Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 10},
                    {.FirstRing = 1, .RingCount = 1, .LevelM = 20}},
       .Rings = {{.FirstPoint = 0, .PointCount = 3}, {.FirstPoint = 3, .PointCount = 4}},
       .Courses = {},
       .Levels = {},
       .Points =
           {-.001, -.001, -.001, .001, .001, 0, .004, .004, .006, .006, .006, .004, .004, .006},
       .Tiles = {},
       .Sources = {},
       .Inputs = {}});
  const auto frame = TangentFrame::At({});
  const auto prepared = PrepareWaterSurfaces(source, frame);
  CHECK(prepared && prepared->SurfacesReady(),
        "an invalid neighboring source polygon does not block valid water");
  if (!prepared) { return Report(); }
  const auto *surfaces = prepared->PreparedSurfaces();
  CHECK(surfaces && surfaces->Mesh.Faces().size() == 1 && surfaces->Rejections.size() == 1 &&
            surfaces->Rejections[0].Surface == 1 &&
            surfaces->Rejections[0].Reason == WaterAsset::SurfaceRejectionReason::InvalidTopology,
        "the omitted source index and concrete topology failure remain in the cooked asset");
  CHECK(prepared->Surfaces().size() == 2 && prepared->SharesInputsWith(source) &&
            prepared->HeapBytes() - source.HeapBytes() == prepared->PreparedSurfaceBytes(),
        "all source information is shared and the added ready payload is counted separately");
  Geometry geometry;
  const auto material = geometry.addSurface("water", WaterSurfaceMaterial());
  CHECK(material, "water material exists");
  if (material) {
    const auto drawn = AppendWaterSurfaceGeometry(geometry, *material, *prepared, frame);
    CHECK(drawn && drawn->Laid == 1 && drawn->Triangles == 1 && drawn->RefusedTopology == 1,
          "rendering reports the local omission and still draws the valid surface");
  }
  return Report();
}
