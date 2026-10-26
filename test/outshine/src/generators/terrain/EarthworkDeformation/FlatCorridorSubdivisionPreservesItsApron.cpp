#include "Check.h"
#include "EarthworkDeformation.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
std::vector<outshine::EarthworkStamp> Strip(size_t parts) {
  std::vector<outshine::EarthworkStamp> result;
  for (size_t part = 0; part < parts; ++part) {
    const double low = -100 + 200.0 * part / parts;
    const double high = -100 + 200.0 * (part + 1) / parts;
    result.push_back({.RingEastNorthM = {low, -1, high, -1, high, 1, low, 1},
                      .HoleRingsEastNorthM = {},
                      .LowE = low,
                      .HighE = high,
                      .LowN = -1,
                      .HighN = 1,
                      .SeamEastNorthM = {},
                      .Profile = {},
                      .PlanarSupportM = {},
                      .CorridorKey = 42,
                      .ApronM = 4.644284786900105,
                      .YieldM = 2.1628565246000697,
                      .Fills = true,
                      .Kind = outshine::EarthworkKind::Corridor});
  }
  return result;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double sourceHeight = 2.1628565246000697;
  constexpr double stepM = 0.002;
  std::vector<EastNorth> points;
  for (int node = 0; node <= 5000; ++node) {
    points.push_back({.EastM = 0, .NorthM = 1 + node * stepM});
  }
  std::vector<double> expected(points.size(), sourceHeight);
  (void)ApplyEarthworkStamps(Strip(1), points, expected, kMostEarthworkM);
  for (const size_t parts : {2u, 8u, 32u, 128u}) {
    std::vector<double> heights(points.size(), sourceHeight);
    const auto result = ApplyEarthworkStamps(Strip(parts), points, heights, kMostEarthworkM);
    double mostDifference = 0;
    double mostSlope = 0;
    for (size_t node = 0; node < heights.size(); ++node) {
      mostDifference = std::max(mostDifference, std::abs(heights[node] - expected[node]));
      if (node != 0) {
        mostSlope = std::max(mostSlope, std::abs(heights[node] - heights[node - 1]) / stepM);
      }
    }
    CHECK(result.Structures == 0 && mostDifference < 1.e-10,
          "subdividing the same horizontal road cannot deepen its terrain apron");
    CHECK(mostSlope <= kBatterRise + 0.0001,
          "fragment count cannot amplify the bounded physical bank slope");
  }
  return Report();
}
