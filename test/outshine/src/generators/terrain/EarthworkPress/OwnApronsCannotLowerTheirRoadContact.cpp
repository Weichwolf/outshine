#include "Check.h"
#include "EarthworkPress.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

outshine::EarthworkStamp Road(double lowE, double highE, uint64_t key) {
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {lowE, -2, highE, -2, highE, 2, lowE, 2};
  stamp.LowE = lowE;
  stamp.HighE = highE;
  stamp.LowN = -2;
  stamp.HighN = 2;
  stamp.PlateauM = 10;
  stamp.CorridorKey = key;
  stamp.ApronM = 30;
  stamp.YieldM = 10;
  stamp.Fills = true;
  stamp.Kind = outshine::EarthworkKind::Corridor;
  return stamp;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<EastNorth, 1> point{{{.EastM = 0, .NorthM = 0}}};
  for (const bool reverse : {false, true}) {
    for (const uint64_t neighbor : {1u, 2u}) {
      std::array stamps{Road(-20, 20, 1), Road(20, 40, neighbor)};
      if (reverse) { std::ranges::reverse(stamps); }
      std::array<double, 1> heights{0};
      const auto result = ApplyEarthworkStamps(stamps, point, heights, kMostEarthworkM);
      CHECK(result.Structures == 0, "all proposed contacts fit the earthwork bound");
      if (neighbor == 1) {
        CHECK(std::abs(heights[0] - 10) < 0.00001,
              "a neighboring span of the same road cannot lower its core contact");
      } else {
        CHECK(heights[0] > 0 && heights[0] < 9,
              "other corridors retain their terrain constraints in either input order");
      }
    }
    EarthworkStamp ceiling = Road(-20, 20, 0);
    ceiling.Kind = EarthworkKind::Clearance;
    ceiling.PlateauM = 8;
    ceiling.ApronM = 0;
    std::array stamps{Road(-20, 20, 1), Road(20, 40, 1), ceiling};
    if (reverse) { std::ranges::reverse(stamps); }
    std::array<double, 1> heights{0};
    (void)ApplyEarthworkStamps(stamps, point, heights, kMostEarthworkM);
    CHECK(heights[0] == 8, "true clearance ceilings continue to limit road support fill");
  }
  const std::array stamps{Road(-20, 20, 1), Road(20, 40, 1)};
  const std::array<EastNorth, 3> shoulder{
      {{.EastM = 0, .NorthM = 1.999}, {.EastM = 0, .NorthM = 2}, {.EastM = 0, .NorthM = 2.001}}};
  std::array<double, 3> heights{0, 0, 0};
  (void)ApplyEarthworkStamps(stamps, shoulder, heights, kMostEarthworkM);
  CHECK(std::abs(heights[0] - heights[1]) < 0.00001 && std::abs(heights[1] - heights[2]) < 0.00001,
        "ownership applies through the shoulder apron without a jump at the core edge");
  auto incline = Road(-20, 20, 1);
  incline.SlopeE = 0.25;
  const std::array<EastNorth, 2> ends{{{.EastM = -21, .NorthM = 0}, {.EastM = 21, .NorthM = 0}}};
  const double part = 1 / std::hypot(30, 1.875 * 10 * 1.5);
  const double fade = part * part * part * (part * (6 * part - 15) + 10);
  for (int rotation = 0; rotation < 4; ++rotation) {
    std::array<double, 2> endHeights{0, 0};
    (void)ApplyEarthworkStamps(std::span(&incline, 1), ends, endHeights, kMostEarthworkM);
    CHECK(std::abs(endHeights[0] - 5 * (1 - fade)) < 0.00001 &&
              std::abs(endHeights[1] - 15 * (1 - fade)) < 0.00001,
          "end aprons use finite contact heights independently of polygon start vertex");
    std::rotate(incline.RingEastNorthM.begin(),
                incline.RingEastNorthM.begin() + 2,
                incline.RingEastNorthM.end());
  }
  return Report();
}
