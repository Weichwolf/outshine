#include "BuildingMesh.h"
#include "StructureSurfaceRefinement.h"
#include "Geodesy.h"
#include "math/Units.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  Vec3 anchor;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 0}, anchor);
  const double longitudeScale = kMPerDegLon * std::cos(47 * kDeg2Rad);
  const std::array<double, 8> ring{47,
                                   9,
                                   47,
                                   9 + 20 / longitudeScale,
                                   47 + 30 / kMPerDegLat,
                                   9 + 20 / longitudeScale,
                                   47 + 30 / kMPerDegLat,
                                   9};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.AnchorEcef = anchor;
  plan.HeightM = 9;
  plan.HeightMeasured = true;
  plan.PitchedShare = 0;
  plan.Coarseness = LevelOfDetail::Fine;
  BuildingMesh generator;
  auto scratch = generator.Scratch();
  Raised reference;
  CHECK(generator.Mesh(plan, *scratch, reference), "native building geometry exists");
  CHECK(!reference.WallRun.empty() && !reference.RoofRun.empty(),
        "both surface streams participate");
  Raised reordered = reference;
  std::ranges::reverse(reordered.WallRun);
  std::ranges::reverse(reordered.RoofRun);
  StructureSurfaceRefinementTask task;
  CHECK(task.Reset({reference, reordered, 53}), "reordered self geometry starts");
  bool finished = false;
  bool safe = true;
  for (size_t slice = 0; slice < 2000; ++slice) {
    const auto progress = task.Step(53);
    if (const auto bound = task.Bound(53)) { safe &= bound->LowerDistanceM() == 0; }
    if (!progress || *progress == StructureSurfaceErrorProgress::Complete) {
      finished = progress.has_value();
      break;
    }
  }
  const auto bound = task.Bound(53);
  CHECK(finished && safe && bound && bound->UpperDistanceM() <= 0.02,
        "native equivalent surfaces obtain useful conservative precision under unchanged caps");
  CHECK(task.TriangleQueries() <= 131072 && task.RegionCount() <= 4096,
        "native utility does not bypass work or scratch limits");
  return Report();
}
