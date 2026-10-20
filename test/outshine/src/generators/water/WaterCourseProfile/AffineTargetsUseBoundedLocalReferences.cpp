#include "WaterCourseProfile.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

WaterCourseSegment Segment(double x, double y, double height) {
  return {.Points = {{{.EastM = x, .NorthM = y}, {.EastM = x + 10, .NorthM = y}}},
          .ModelHeightsM = {height, height + 10}};
}

void CheckAffine() {
  const std::array segments{Segment(0, 0, 100), Segment(10, 0, 110)};
  const auto profile = WaterCourseProfile::Build(segments);
  CHECK(profile.has_value(), "finite nonzero segments build one reusable hierarchy");
  if (!profile) { return; }
  for (const double x : {-20.0, 0.0, 5.0, 10.0, 15.0, 20.0, 40.0}) {
    const auto sample = profile->Sample({.EastM = x, .NorthM = 7});
    CHECK(sample && std::abs(sample->ModelHeightM - (100 + x)) < 1e-12,
          "affine targets reproduce a plane beyond segment endpoints without clamping heights");
    CHECK(sample && sample->References == 2 && sample->SegmentTests > 0,
          "both locally overlapping supports contribute with measured work");
  }
  const auto tie = profile->Sample({.EastM = 10, .NorthM = 0});
  CHECK(tie && tie->NearestSegment == 0, "equal-distance ties retain source order");
  CHECK(!profile->Sample({.EastM = 10, .NorthM = 0}, 12, 1),
        "local reference exhaustion is explicit rather than silently selecting a subset");
}

void CheckBoundaries() {
  const std::array segments{Segment(0, 0, 100)};
  auto profile = WaterCourseProfile::Build(segments);
  CHECK(profile.has_value(), "one segment is sufficient");
  if (!profile) { return; }
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!profile->Sample({.EastM = nan, .NorthM = 0}) &&
            !profile->Sample({.EastM = 0, .NorthM = 0}, 0) &&
            !profile->Sample({.EastM = 0, .NorthM = 0}, 12, 0),
        "query coordinates and work limits are validated");
  const double extreme = std::numeric_limits<double>::max();
  CHECK(!profile->Sample({.EastM = extreme, .NorthM = extreme}),
        "unrepresentable projections cannot publish a finite-looking target");
  CHECK(!WaterCourseProfile::Build({}) && !WaterCourseProfile::Build(segments, 0),
        "empty input and source-budget exhaustion are rejected");
  auto invalid = segments;
  invalid[0].Points[1] = invalid[0].Points[0];
  CHECK(!WaterCourseProfile::Build(invalid), "zero length cannot enter projection division");
  invalid = segments;
  invalid[0].ModelHeightsM[0] = nan;
  CHECK(!WaterCourseProfile::Build(invalid), "nonfinite source heights are rejected");
}

void CheckSelection() {
  constexpr uint32_t count = 512;
  std::vector<WaterCourseSegment> segments;
  for (uint32_t at = 0; at < count; ++at) {
    segments.push_back(Segment(static_cast<double>(at) * 100, 0, static_cast<double>(at)));
  }
  const auto profile = WaterCourseProfile::Build(segments);
  CHECK(profile.has_value(), "spatially separated segments build without expanding query storage");
  if (!profile) { return; }
  for (const uint32_t at : {0u, 17u, 255u, 511u}) {
    const auto sample = profile->Sample({.EastM = static_cast<double>(at) * 100 + 5, .NorthM = 0});
    CHECK(sample && sample->NearestSegment == at && sample->References == 1 &&
              sample->ModelHeightM == static_cast<double>(at) + 5,
          "hierarchy selection preserves the exact nearest segment and local affine target");
    CHECK(sample && sample->SegmentTests < count / 8,
          "local queries avoid a full source-table scan");
  }
}
}

int main() {
  CheckAffine();
  CheckBoundaries();
  CheckSelection();
  return outshine::Test::Report();
}
