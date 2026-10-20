#include "Check.h"
#include "EarthworkPress.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {
outshine::EarthworkStamp Road(double lowN, double height) {
  return {.RingEastNorthM = {-50, lowN, 50, lowN, 50, lowN + 4, -50, lowN + 4},
          .LowE = -50,
          .HighE = 50,
          .LowN = lowN,
          .HighN = lowN + 4,
          .PlateauM = height,
          .CorridorKey = 42,
          .ApronM = 20,
          .YieldM = 4,
          .Fills = true,
          .Kind = outshine::EarthworkKind::Corridor};
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double step = 0.004;
  const std::array roads{Road(-12, -4), Road(8, 0)};
  std::vector<EastNorth> points;
  for (int index = 0; index <= 8000; ++index) {
    points.push_back({.EastM = 0, .NorthM = -16 + index * step});
  }
  std::vector<double> heights(points.size(), -2);
  const auto result = ApplyEarthworkStamps(roads, points, heights, kMostEarthworkM);
  CHECK(result.Structures == 0, "both road heights are physically admitted");
  double greatestSlope = 0;
  for (size_t at = 0; at < points.size(); ++at) {
    const double north = points[at].NorthM;
    if (north >= -12 && north <= -8) {
      CHECK(std::abs(heights[at] + 4) < 1.e-10, "the lower physical road keeps its contact");
    }
    if (north >= 8 && north <= 12) {
      CHECK(std::abs(heights[at]) < 1.e-10, "the upper physical road keeps its contact");
    }
    if (at != 0 && north > -8 && north < 8) {
      greatestSlope = std::max(greatestSlope, std::abs(heights[at] - heights[at - 1]) / step);
    }
  }
  CHECK(greatestSlope <= kBatterRise + 0.0001,
        "a shared influence envelope joins different heights without a Voronoi step");
  std::vector<double> staged(points.size(), -2);
  EarthworkPressJob job(roads, points, staged, kMostEarthworkM);
  while (!job.Advance(1)) {}
  CHECK(staged == heights, "group scratch remains independent of the job's point slice");
  const std::array reversed{roads[1], roads[0]};
  std::vector<double> reordered(points.size(), -2);
  (void)ApplyEarthworkStamps(reversed, points, reordered, kMostEarthworkM);
  double mostDifference = 0;
  for (size_t at = 0; at < heights.size(); ++at) {
    mostDifference = std::max(mostDifference, std::abs(heights[at] - reordered[at]));
  }
  CHECK(mostDifference < 1.e-10, "road enumeration cannot change the grouped terrain surface");
  return Report();
}
