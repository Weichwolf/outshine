#include "Check.h"
#include "EarthworkPress.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace outshine;
using outshine::Test::Report;

namespace {
EarthworkStamp Clearance(bool fills) {
  EarthworkStamp stamp;
  stamp.Kind = EarthworkKind::Clearance;
  stamp.RingEastNorthM = {-1, -1, 1, -1, 1, 1, -1, 1};
  stamp.LowE = stamp.LowN = -1;
  stamp.HighE = stamp.HighN = 1;
  stamp.ApronM = 6;
  stamp.Fills = fills;
  return stamp;
}
}

int main() {
  std::vector<EastNorth> points;
  constexpr double stepM = 0.02;
  for (size_t i = 0; i <= 4000; ++i) {
    points.push_back({.EastM = static_cast<double>(i) * stepM, .NorthM = 0});
  }
  for (const bool fills : {false, true}) {
    const std::array stamps{Clearance(fills)};
    std::vector<double> heights(points.size(), 20.0);
    const auto result = ApplyEarthworkStamps(stamps, points, heights, 30.0);
    CHECK(result.Held == 0 && heights.front() == 0 && heights.back() == 20,
          "clearance preserves its core ceiling and returns to unchanged source terrain");
    double mostGradient = 0, mostGradientStep = 0, previous = 0;
    for (size_t i = 1; i < heights.size(); ++i) {
      const double gradient = (heights[i] - heights[i - 1]) / stepM;
      mostGradient = std::max(mostGradient, std::abs(gradient));
      mostGradientStep = std::max(mostGradientStep, std::abs(gradient - previous));
      previous = gradient;
    }
    CHECK(mostGradient <= 2.0 / 3.0 && mostGradientStep < 0.01,
          "an undeclared source relief plans a continuous bounded clearance transition");
    std::vector<double> below(points.size(), -2.0);
    (void)ApplyEarthworkStamps(stamps, points, below, 30.0);
    CHECK(std::ranges::all_of(below, [](double h) { return h == -2.0; }),
          "clearance never fills a core or apron even with an incorrect fills flag");
    auto staged = std::vector<double>(points.size(), 20.0);
    EarthworkPressJob job(stamps, points, staged, 30.0);
    while (!job.Advance(17)) {}
    CHECK(staged == heights, "bounded slices preserve the complete clearance result");
    std::vector<double> impossible(points.size(), 31.0);
    const auto rejected = ApplyEarthworkStamps(stamps, points, impossible, 30.0);
    CHECK(rejected.Refused.size() == 1 && rejected.Refused.front() == 1 &&
              std::ranges::all_of(impossible, [](double h) { return h == 31.0; }),
          "softening an apron cannot hide an impossible core clearance");
  }
  auto road = Clearance(true);
  road.Kind = EarthworkKind::Pad;
  road.RingEastNorthM = {9, -1, 11, -1, 11, 1, 9, 1};
  road.LowE = 9;
  road.HighE = 11;
  road.ApronM = 0;
  road.PlateauM = 5;
  const std::array<EastNorth, 2> contacts{{{.EastM = 0, .NorthM = 0}, {.EastM = 10, .NorthM = 0}}};
  for (const bool reverse : {false, true}) {
    std::array stamps{Clearance(false), road};
    if (reverse) { std::ranges::reverse(stamps); }
    std::array<double, 2> heights{20, 20};
    (void)ApplyEarthworkStamps(stamps, contacts, heights, 30.0);
    CHECK(heights[0] == 0 && heights[1] == 5,
          "genuine clearance remains a ceiling while its outer apron respects physical contacts");
  }
  return Report();
}
