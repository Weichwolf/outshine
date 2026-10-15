#include "ClassificationRasterizer.h"
#include "ClassStructure.h"
#include "Check.h"
#include <cmath>
#include <limits>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Rasterizer = Generators::ClassificationRasterizer;
  Rasterizer::Input input;
  input.CellM = 2;
  input.HalfCells = 16;
  input.BoundaryHaloM = 2.5;
  input.Points = {-12, -12, 12, -12, 12, 12, -12, 12, -4, -4, -4, 4, 4, 4, 4, -4};
  input.Rings = {{0, 4}, {4, 4}};
  input.Features = {{.FirstRing = 0,
                     .RingCount = 2,
                     .Rank = 10,
                     .ClassRow = 3,
                     .MinE = -12,
                     .MinN = -12,
                     .MaxE = 12,
                     .MaxN = 12}};
  Rasterizer rasterizer;
  const auto built = rasterizer.Build(input);
  CHECK(built && built->Overflow == 0 && built->Grid->BoundaryHaloM == 2.5,
        "the halo fits the unchanged aggregate seed/reference budget");
  if (!built) { return Report(); }
  const auto empty = std::make_shared<const ClassStructure::Grid>();
  const ClassStructure field(TangentFrame::At({}), built->Grid, empty, {});
  for (double north = -15.5; north < 16; north += 1) {
    for (double east = -15.5; east < 16; east += 1) {
      const bool outer = std::abs(east) < 12 && std::abs(north) < 12;
      const bool hole = std::abs(east) < 4 && std::abs(north) < 4;
      CHECK(field.Evaluate(east, north, nullptr, nullptr) == (outer && !hole ? 3 : -1),
            "halo hints do not extend functional coverage or close holes");
    }
  }
  for (double distance : {-.75, -.25, .25, .75}) {
    const auto outer = field.MaterialAt(0, -12 + distance);
    const auto hole = field.MaterialAt(0, -4 - distance);
    CHECK(outer.Class == 3 && outer.Other == -1 && hole.Class == 3 && hole.Other == -1,
          "inside and outside share the same two materials across outer and hole boundaries");
    CHECK_NEAR(outer.SignedEdgeM,
               distance,
               1e-6,
               "outer signed distance",
               "a cell boundary does not cut off the neighbouring boundary hint");
    CHECK_NEAR(hole.SignedEdgeM,
               distance,
               1e-6,
               "hole signed distance",
               "holes reverse the boundary normal without reversing material weights");
  }
  CHECK(field.MaterialAt(0, -15).Class == -1,
        "points outside the declared halo do not fabricate a neighbouring material");
  CHECK(field.MaterialAt(std::numeric_limits<double>::infinity(), 0).Class == -1,
        "nonfinite material queries cannot reach raster indices");
  auto coarseInput = input;
  coarseInput.CellM = 8;
  coarseInput.HalfCells = 4;
  coarseInput.BoundaryHaloM = 5.5;
  const auto coarse = rasterizer.Build(coarseInput);
  CHECK(coarse && coarse->Overflow == 0, "the same coverage has a coarser footprint product");
  if (coarse) {
    const ClassStructure tiers(TangentFrame::At({}), built->Grid, coarse->Grid, {});
    const auto selected = tiers.MaterialAt(0, -12.25, 2);
    CHECK(selected.Class == 3 && selected.HaloM == 5.5 && selected.SignedEdgeM == -.25,
          "a wide pixel selects the supported coarse field before neighbour evaluation");
    CHECK(tiers.MaterialAt(0, -12.25, 5).Class == -1,
          "a pixel beyond both declared supports uses the regional appearance proxy");
  }
  input.Points.insert(input.Points.end(), {-30, -30, 30, -30, 30, 30, -30, 30});
  input.Rings.push_back({8, 4});
  input.Features.push_back({.FirstRing = 2,
                            .RingCount = 1,
                            .Rank = 20,
                            .ClassRow = 4,
                            .MinE = -30,
                            .MinN = -30,
                            .MaxE = 30,
                            .MaxN = 30});
  const auto overlaid = rasterizer.Build(input);
  CHECK(overlaid && overlaid->Overflow == 0, "the higher material stays within the same budget");
  if (overlaid) {
    const ClassStructure opaque(TangentFrame::At({}), overlaid->Grid, empty, {});
    const auto material = opaque.MaterialAt(0, -12.25);
    CHECK(opaque.Evaluate(0, -12.25, nullptr, nullptr) == 4 && material.Class == 4 &&
              material.SignedEdgeM == ClassStructure::kNoEdgeM,
          "an opaque higher-ranked interior suppresses a hidden lower boundary hint");
  }
  input.BoundaryHaloM = -1;
  CHECK(!rasterizer.Build(input), "negative halo extents cannot publish");
  return Report();
}
