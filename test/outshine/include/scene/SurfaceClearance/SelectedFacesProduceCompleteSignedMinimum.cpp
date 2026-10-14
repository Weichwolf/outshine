#include <scene/SurfaceClearance.h>

#include "Check.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Face = std::array<uint32_t, 3>;

void CheckFailure(HeightSurfaceView lower,
                  HeightSurfaceView upper,
                  SurfaceClearanceError error,
                  SurfaceClearanceLimits limits = {}) {
  const auto result = MeasureSurfaceClearance(lower, upper, limits);
  CHECK(!result && result.error().Error == error,
        "failed complete measurement exposes no partial numeric minimum");
}

void CheckSpatialBudgets() {
  const std::array<Vec3, 3> lower{{{{0, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}}}};
  const std::array<Face, 1> faces{{{0, 1, 2}}};
  std::vector<Vec3> upper;
  std::vector<Face> upperFaces;
  for (uint32_t at = 0; at < 32; ++at) {
    const double offset = at * 10.0;
    const auto base = static_cast<uint32_t>(upper.size());
    upper.insert(upper.end(), {{{offset, 0, 3}}, {{offset + 1, 0, 3}}, {{offset, 1, 3}}});
    upperFaces.push_back({base, base + 1, base + 2});
  }
  const auto result = MeasureSurfaceClearance(
      {lower, faces}, {upper, upperFaces}, {.MaxCandidateTests = 1, .MaxPairs = 1});
  CHECK(result && result->Pairs == 1 && result->CandidateTests == 1 && result->MinimumM == 3,
        "distant selected faces do not consume projected pair or candidate budgets");
  const std::array<Vec3, 3> disjoint{{{{3, 0, 2}}, {{4, 0, 2}}, {{3, 1, 2}}}};
  CheckFailure({lower, faces},
               {disjoint, faces},
               SurfaceClearanceError::NoOverlap,
               {.MaxCandidateTests = 0, .MaxPairs = 0});
  CheckFailure(
      {lower, faces}, {upper, upperFaces}, SurfaceClearanceError::InputBudget, {.MaxVertices = 3});
  upperFaces.back()[2] = static_cast<uint32_t>(upper.size());
  CheckFailure({lower, faces}, {upper, upperFaces}, SurfaceClearanceError::InvalidInput);
}

void CheckThicknessAndSmallOverlap() {
  const std::array<Vec3, 4> lower{{{{0, 0, 0}}, {{2, 0, 0}}, {{2, 2, .04}}, {{0, 2, .04}}}};
  const std::array<Vec3, 4> upper{{{{0, 0, 4.25}}, {{2, 0, 4.25}}, {{2, 2, 4.21}}, {{0, 2, 4.21}}}};
  const std::array<Face, 2> faces{{{0, 1, 2}, {0, 2, 3}}};
  const auto result = MeasureSurfaceClearance({lower, faces}, {upper, faces});
  CHECK(result.has_value(), "opposed crossfalls include the selected deck underside");
  if (result) {
    CHECK_NEAR(result->MinimumM,
               4.17,
               1e-12,
               "meters",
               "whole-width clearance includes crossfalls and preselected slab thickness");
  }
  auto shifted = upper;
  for (auto &point : shifted) { point[0] += 2.0; }
  CheckFailure({lower, faces}, {shifted, faces}, SurfaceClearanceError::NoOverlap);
  for (auto &point : shifted) { point[0] -= 1e-13; }
  CHECK(MeasureSurfaceClearance({lower, faces}, {shifted, faces}).has_value(),
        "tiny positive overlap is included rather than mistaken for edge-only contact");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CheckSpatialBudgets();
  CheckThicknessAndSmallOverlap();
  const std::array<Vec3, 3> lower{{{{0, 0, 0}}, {{4, 0, 0}}, {{0, 4, 0}}}};
  std::array<Vec3, 3> upper{{{{1, -1, 3.75}}, {{5, 3, 2.75}}, {{1, 5, .75}}}};
  const std::array<Face, 1> faces{{{0, 1, 2}}};
  const HeightSurfaceView low{lower, faces};
  const auto check = [&](double expected) {
    const auto result = MeasureSurfaceClearance(low, {upper, faces});
    CHECK(result.has_value(), "selected sloped faces have a complete projected-area minimum");
    if (!result) { return; }
    CHECK_NEAR(result->MinimumM,
               expected,
               1e-12,
               "meters",
               "affine gap is smallest at the clipped polygon vertex");
    CHECK_NEAR(
        result->Witness[0], 1, 1e-12, "east meters", "witness is on the actual intersection");
    CHECK_NEAR(result->Witness[1],
               3,
               1e-12,
               "north meters",
               "witness need not be an original input vertex");
    CHECK(result->LowerFace == 0 && result->UpperFace == 0 && result->Pairs == 1 &&
              result->Overlaps == 1,
          "original face identity and actual projected pair counts survive measurement");
  };
  check(1.75);
  for (auto &position : upper) { position[2] -= 6; }
  check(-4.25);
  const std::array<Face, 2> repeated{{{0, 1, 2}, {2, 1, 0}}};
  const auto duplicate = MeasureSurfaceClearance(low, {upper, repeated});
  CHECK(duplicate && duplicate->UpperFace == 0 && duplicate->Pairs == 2,
        "reversed and repeated selected surfaces keep deterministic lowest-index ties");
  CheckFailure(low, {upper, repeated}, SurfaceClearanceError::PairBudget, {.MaxPairs = 1});
  CheckFailure(
      low, {upper, faces}, SurfaceClearanceError::CandidateBudget, {.MaxCandidateTests = 0});
  CheckFailure(low, {upper, faces}, SurfaceClearanceError::InputBudget, {.MaxFaces = 0});
  CheckFailure(low,
               {upper, faces},
               SurfaceClearanceError::Deadline,
               {.Deadline = std::chrono::steady_clock::time_point::min()});
  const std::array<Vec3, 3> touching{{{{4, 0, 2}}, {{5, 0, 2}}, {{4, 1, 2}}}};
  CheckFailure(low, {touching, faces}, SurfaceClearanceError::NoOverlap);
  auto invalid = upper;
  invalid[1][0] = std::numeric_limits<double>::quiet_NaN();
  CheckFailure(low, {invalid, faces}, SurfaceClearanceError::InvalidInput);
  const std::array<Face, 1> outside{{{0, 1, 3}}};
  CheckFailure(low, {upper, outside}, SurfaceClearanceError::InvalidInput);
  const std::array<Vec3, 3> vertical{{{{0, 0, 0}}, {{0, 0, 1}}, {{0, 1, 1}}}};
  CheckFailure(low, {vertical, faces}, SurfaceClearanceError::InvalidInput);
  auto deep = lower;
  auto high = lower;
  for (auto &position : deep) { position[2] = -std::numeric_limits<double>::max(); }
  for (auto &position : high) { position[2] = std::numeric_limits<double>::max(); }
  CheckFailure({deep, faces}, {high, faces}, SurfaceClearanceError::Unrepresentable);
  CHECK(lower[0] == Vec3{} && upper[0][2] == -2.25,
        "successful and failed measurements preserve borrowed input buffers");
  return Report();
}
