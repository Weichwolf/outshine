#include "Check.h"
#include "EarthworkPress.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace {

outshine::EarthworkStamp Contact(double heightM, bool profiled) {
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {-50, -2, 50, -2, 50, 2, -50, 2};
  stamp.LowE = -50;
  stamp.HighE = 50;
  stamp.LowN = -2;
  stamp.HighN = 2;
  stamp.PlateauM = heightM;
  stamp.ApronM = 6;
  stamp.YieldM = std::abs(heightM);
  stamp.Fills = true;
  stamp.Kind = outshine::EarthworkKind::Corridor;
  if (profiled) {
    stamp.Profile =
        outshine::ProfiledCorridorSpan{.CorridorKey = 1,
                                       .BeginM = {.EastM = -50, .NorthM = 0},
                                       .EndM = {.EastM = 50, .NorthM = 0},
                                       .BeginDerivativeM = {.EastM = 100, .NorthM = 0, .UpM = 0},
                                       .EndDerivativeM = {.EastM = 100, .NorthM = 0, .UpM = 0},
                                       .StationLengthM = 100,
                                       .BeginBedM = heightM,
                                       .EndBedM = heightM,
                                       .BeginPavementHalfWidthM = 2,
                                       .EndPavementHalfWidthM = 2,
                                       .BeginHalfWidthM = 2,
                                       .EndHalfWidthM = 2};
  }
  return stamp;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double stepM = 0.01;
  std::vector<EastNorth> points;
  for (int sample = 0; sample <= 10000; ++sample) {
    points.push_back({.EastM = 0, .NorthM = static_cast<double>(sample) * stepM});
  }
  for (const bool profiled : {false, true}) {
    for (const double bedM : {-20., -6., -.5, .5, 6., 20.}) {
      const std::array stamps{Contact(bedM, profiled)};
      std::vector<double> heights(points.size(), 0);
      const auto result = ApplyEarthworkStamps(stamps, points, heights, kMostEarthworkM);
      double maximumSlope = 0;
      double maximumSlopeChange = 0;
      double previousSlope = 0;
      for (size_t at = 1; at < heights.size(); ++at) {
        const double slope = (heights[at] - heights[at - 1]) / stepM;
        maximumSlope = std::max(maximumSlope, std::abs(slope));
        maximumSlopeChange = std::max(maximumSlopeChange, std::abs(slope - previousSlope));
        previousSlope = slope;
      }
      CHECK(result.Structures == 0 && heights.front() == bedM && heights.back() == 0,
            "cut and fill keep contact height and return to unmodified source terrain");
      CHECK(maximumSlope <= kBatterRise + 0.0001 && maximumSlopeChange < 0.01,
            "road and junction aprons have bounded slopes without height or slope steps");
      CHECK(std::ranges::all_of(
                heights,
                [bedM](double h) { return h >= std::min(0., bedM) && h <= std::max(0., bedM); }),
            "smooth earthwork introduces no height overshoot");
    }
  }
  std::array roads{Contact(0, true), Contact(6, true)};
  for (size_t road = 0; road < roads.size(); ++road) {
    const double northM = road == 0 ? -12.0 : 12.0;
    auto &stamp = roads[road];
    stamp.Profile->CorridorKey = road + 1;
    stamp.Profile->BeginM.NorthM = stamp.Profile->EndM.NorthM = northM;
    stamp.LowN += northM;
    stamp.HighN += northM;
    for (size_t at = 1; at < stamp.RingEastNorthM.size(); at += 2) {
      stamp.RingEastNorthM[at] += northM;
    }
  }
  const std::array<EastNorth, 3> between{
      {{.EastM = 0, .NorthM = -0.001}, {.EastM = 0, .NorthM = 0}, {.EastM = 0, .NorthM = 0.001}}};
  std::array<double, 3> blended{};
  (void)ApplyEarthworkStamps(roads, between, blended, kMostEarthworkM);
  CHECK(std::abs(blended[0] - blended[1]) < 0.001 && std::abs(blended[1] - blended[2]) < 0.001,
        "overlapping aprons do not jump when the nearest road changes");
  EarthworkStamp basin;
  basin.Kind = EarthworkKind::Basin;
  basin.RingEastNorthM = {-50, 8, 50, 8, 50, 100, -50, 100};
  basin.LowE = -50;
  basin.HighE = 50;
  basin.LowN = 8;
  basin.HighN = 100;
  basin.PlateauM = -2;
  const std::array<EastNorth, 2> shores{{{.EastM = 0, .NorthM = 9}, {.EastM = 0, .NorthM = 10}}};
  for (const bool profiled : {false, true}) {
    std::array stamps{Contact(6, profiled), basin};
    for (const bool reverse : {false, true}) {
      if (reverse) { std::ranges::reverse(stamps); }
      std::array<double, 2> heights{};
      (void)ApplyEarthworkStamps(stamps, shores, heights, kMostEarthworkM);
      CHECK(heights[0] == -2 && heights[1] == -2,
            "a soft road apron never fills the neighbouring water basin in either order");
    }
  }
  return Report();
}
