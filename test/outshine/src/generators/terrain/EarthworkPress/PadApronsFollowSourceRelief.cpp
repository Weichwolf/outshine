#include "Check.h"
#include "EarthworkPress.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

outshine::EarthworkStamp Pad(double apronM = 6) {
  return {.RingEastNorthM = {-4, -4, 4, -4, 4, 4, -4, 4},
          .LowE = -4,
          .HighE = 4,
          .LowN = -4,
          .HighN = 4,
          .ApronM = apronM,
          .Fills = true,
          .Kind = outshine::EarthworkKind::Pad};
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double stepM = 0.02;
  std::vector<EastNorth> points;
  for (int sample = 0; sample <= 4000; ++sample) {
    points.push_back({.EastM = 0, .NorthM = sample * stepM});
  }
  for (const double sourceM : {-20., 20.}) {
    const std::array stamps{Pad()};
    std::vector<double> heights(points.size(), sourceM);
    const auto pressed = ApplyEarthworkStamps(stamps, points, heights, kMostEarthworkM);
    CHECK(pressed.Structures == 0 && heights.front() == 0 && heights.back() == sourceM,
          "observed cut and fill preserve the foundation and return to the source");
    CHECK(std::abs(heights[500]) < std::abs(sourceM) * 0.1,
          "underestimated metadata cannot restrict the observed relief to a six metre apron");
    double maximumSlope = 0;
    double maximumSlopeChange = 0;
    double previousSlope = 0;
    for (size_t at = 1; at < heights.size(); ++at) {
      const double slope = (heights[at] - heights[at - 1]) / stepM;
      maximumSlope = std::max(maximumSlope, std::abs(slope));
      maximumSlopeChange = std::max(maximumSlopeChange, std::abs(slope - previousSlope));
      previousSlope = slope;
    }
    CHECK(maximumSlope <= kBatterRise + 0.0001 && maximumSlopeChange < 0.01,
          "observed foundation relief produces a bounded smooth apron");
    std::vector<double> sliced(points.size(), sourceM);
    EarthworkPressJob job(stamps, points, sliced, kMostEarthworkM);
    while (!job.Advance(1)) {}
    const auto staged = job.Take();
    CHECK(sliced == heights && staged.DecidedBy == pressed.DecidedBy,
          "planning is identical across job slices");
    std::ranges::reverse(points);
    std::vector<double> reversed(points.size(), sourceM);
    (void)ApplyEarthworkStamps(stamps, points, reversed, kMostEarthworkM);
    std::ranges::reverse(reversed);
    CHECK(reversed == heights && stamps.front().YieldM == 0,
          "planning is independent of sample order and leaves the input immutable");
    std::ranges::reverse(points);
  }
  const std::array hard{Pad(0)};
  const std::array<EastNorth, 2> contact{{{.EastM = 0, .NorthM = 0}, {.EastM = 0, .NorthM = 4.01}}};
  std::array<double, 2> heights{20, 20};
  (void)ApplyEarthworkStamps(hard, contact, heights, kMostEarthworkM);
  CHECK(heights[0] == 0 && heights[1] == 20, "an explicit zero apron retains its hard contact");
  const std::array<EastNorth, 3> slopes{
      {{.EastM = 0, .NorthM = 0}, {.EastM = 0, .NorthM = 10}, {.EastM = 0, .NorthM = 24}}};
  const std::array soft{Pad()};
  std::array<double, 3> hillside{5, 5, 100};
  (void)ApplyEarthworkStamps(soft, slopes, hillside, kMostEarthworkM);
  CHECK(hillside[0] == 0 && hillside[1] > 1 && hillside[2] == 100,
        "high terrain outside the physical core cannot enlarge the foundation apron");
  auto declared = Pad();
  declared.YieldM = 20;
  hillside = {5, 5, 100};
  (void)ApplyEarthworkStamps(std::span(&declared, 1), slopes, hillside, kMostEarthworkM);
  CHECK(hillside[1] < 0.1, "declared relief remains a minimum when the sampled core is flatter");
  std::array<double, 2> rejected{31, 31};
  const auto refused = ApplyEarthworkStamps(soft, contact, rejected, kMostEarthworkM);
  CHECK(refused.Structures == 1 && rejected[0] == 31 && rejected[1] == 31,
        "excessive core relief remains rejected before planning or pressing");
  return Report();
}
