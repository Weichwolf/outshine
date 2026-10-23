#include "HeightSolid.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr double kToleranceM3 = 1e-12;
const std::array kPoints{
    PlanarPoint{0, 0}, PlanarPoint{4, 0}, PlanarPoint{4, 3}, PlanarPoint{0, 3}};
const std::array kFaces{PlanarFace{0, 1, 2}, PlanarFace{0, 2, 3}};
const std::array<double, 4> kBottom{};

void SolidCapsAndOwnedRisers() {
  const std::array top{std::array{1., 1., 1.}, std::array{2., 2., 2.}};
  const auto solid = BuildHeightSolid(
      {.Points = kPoints, .Faces = kFaces, .TopHeights = top, .BottomHeights = kBottom});
  CHECK(solid.has_value(),
        "different adjacent cap heights close around the same indexed planar surface");
  if (!solid) { return; }
  CHECK_NEAR(solid->VolumeM3,
             18,
             kToleranceM3,
             "m3",
             "each triangular column retains its analytic positive volume");
  CHECK(solid->SurfaceFaces == kFaces.size() && solid->Faces.size() == 20,
        "two caps, reversed undersides and conforming walls form one closed indexed body");
  CHECK(solid->Work.IntermediateHeights == 2 && solid->Points.size() == 12,
        "tall boundary walls include existing lower step vertices without global height scans");
  size_t risers = 0;
  for (size_t at = 0; at < solid->WallKinds.size(); ++at) {
    if (solid->WallKinds[at] == HeightSolidWallKind::Discontinuity) {
      ++risers;
      CHECK(solid->WallParents[at] == 1, "riser triangles retain their higher source-face owner");
    }
  }
  CHECK(risers == 2 && solid->WallKinds.size() == solid->Faces.size() - 2 * solid->SurfaceFaces,
        "boundary and interior discontinuity metadata cover exactly the wall triangles");
  CHECK(solid->Work.IndexedHeights == 10 && solid->Work.NodeQueries == 12,
        "the immutable height index contains only unique source-point/elevation nodes");
}

void ContinuousAndTriangularTransitions() {
  std::array top{std::array{1., 1., 1.}, std::array{1., 1., 1.}};
  const auto flat = BuildHeightSolid(
      {.Points = kPoints, .Faces = kFaces, .TopHeights = top, .BottomHeights = kBottom});
  CHECK(flat && flat->Points.size() == 8 && flat->Faces.size() == 12,
        "continuous adjacent caps share vertices and introduce no internal wall");
  if (flat) {
    CHECK_NEAR(flat->VolumeM3, 12, kToleranceM3, "m3", "the continuous slab preserves volume");
  }
  top[1] = {1, 2, 2};
  const auto wedge = BuildHeightSolid(
      {.Points = kPoints, .Faces = kFaces, .TopHeights = top, .BottomHeights = kBottom});
  CHECK(wedge.has_value(),
        "a discontinuity ending at one shared-height corner needs a triangular riser");
  if (wedge) {
    CHECK(std::ranges::count(wedge->WallKinds, HeightSolidWallKind::Discontinuity) == 1,
          "one triangular wall closes a step taper without a zero-area extra triangle");
  }
  top[0] = {2, 1, 1};
  const auto crossing = BuildHeightSolid(
      {.Points = kPoints, .Faces = kFaces, .TopHeights = top, .BottomHeights = kBottom});
  CHECK(!crossing && crossing.error() == HeightSolidError::CrossingHeights,
        "crossing cap heights require prior edge refinement instead of a twisted wall");
}

void RefusalsNeverExposePartialBodies() {
  const std::array top{std::array{1., 1., 1.}, std::array{1., 1., 1.}};
  const HeightSolidInput input{
      .Points = kPoints, .Faces = kFaces, .TopHeights = top, .BottomHeights = kBottom};
  for (size_t which = 0; which < 5; ++which) {
    HeightSolidBudget budget;
    switch (which) {
      case 0: budget.Points = 3; break;
      case 1: budget.SurfaceFaces = 1; break;
      case 2: budget.Vertices = 7; break;
      case 3: budget.Faces = 11; break;
      default: budget.EdgeVisits = 1; break;
    }
    const auto refused = BuildHeightSolid(input, budget);
    CHECK(!refused && refused.error() == HeightSolidError::CapacityExceeded,
          "each exhausted work/output budget refuses the entire body");
  }
  std::stop_source canceled;
  canceled.request_stop();
  const auto stopped = BuildHeightSolid(input, {}, canceled.get_token());
  CHECK(!stopped && stopped.error() == HeightSolidError::Canceled,
        "pre-cancellation avoids all geometry work");
  auto duplicate = kFaces;
  duplicate[1] = duplicate[0];
  const auto bad = BuildHeightSolid(
      {.Points = kPoints, .Faces = duplicate, .TopHeights = top, .BottomHeights = kBottom});
  CHECK(!bad && bad.error() == HeightSolidError::NonManifold,
        "repeated directed input edges cannot be silently sealed");
  auto malformed = kPoints;
  malformed[0][0] = std::numeric_limits<double>::quiet_NaN();
  const auto invalid = BuildHeightSolid(
      {.Points = malformed, .Faces = kFaces, .TopHeights = top, .BottomHeights = kBottom});
  CHECK(!invalid && invalid.error() == HeightSolidError::InvalidInput,
        "nonfinite geometry is rejected at the boundary");
  const auto empty = BuildHeightSolid({});
  CHECK(empty && empty->Faces.empty() && empty->Points.empty(),
        "the empty request has an empty neutral result");
}

void HolesRemainOpenAndPinchedBodiesAreRefused() {
  const std::array points{PlanarPoint{0, 0},
                          PlanarPoint{4, 0},
                          PlanarPoint{4, 4},
                          PlanarPoint{0, 4},
                          PlanarPoint{1, 1},
                          PlanarPoint{3, 1},
                          PlanarPoint{3, 3},
                          PlanarPoint{1, 3}};
  const std::array faces{PlanarFace{0, 1, 5},
                         PlanarFace{0, 5, 4},
                         PlanarFace{1, 2, 6},
                         PlanarFace{1, 6, 5},
                         PlanarFace{2, 3, 7},
                         PlanarFace{2, 7, 6},
                         PlanarFace{3, 0, 4},
                         PlanarFace{3, 4, 7}};
  std::array<std::array<double, 3>, faces.size()> tops{};
  tops.fill({1, 1, 1});
  const std::array<double, points.size()> bottoms{};
  const auto ring = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = tops, .BottomHeights = bottoms});
  CHECK(ring && ring->Points.size() == 16 && ring->Faces.size() == 32,
        "inner and outer boundary walls close a holed surface without filling its aperture");
  if (ring) {
    CHECK_NEAR(ring->VolumeM3, 12, kToleranceM3, "m3", "the central hole contributes no volume");
  }
  const std::array pinchPoints{PlanarPoint{0, 0},
                               PlanarPoint{1, 0},
                               PlanarPoint{0, 1},
                               PlanarPoint{-1, 0},
                               PlanarPoint{0, -1}};
  const std::array pinchFaces{PlanarFace{0, 1, 2}, PlanarFace{0, 3, 4}};
  const std::array pinchTops{std::array{1., 1., 1.}, std::array{1., 1., 1.}};
  const std::array<double, pinchPoints.size()> pinchBottoms{};
  const auto pinch = BuildHeightSolid({.Points = pinchPoints,
                                       .Faces = pinchFaces,
                                       .TopHeights = pinchTops,
                                       .BottomHeights = pinchBottoms});
  CHECK(!pinch && pinch.error() == HeightSolidError::NonManifold,
        "edge twins alone cannot admit two disconnected surface fans sharing one vertical edge");
}
}

int main() {
  SolidCapsAndOwnedRisers();
  ContinuousAndTriangularTransitions();
  RefusalsNeverExposePartialBodies();
  HolesRemainOpenAndPinchedBodiesAreRefused();
  return Report();
}
