#include "Check.h"
#include "EarthworkPress.h"

#include <algorithm>
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  EarthworkStamp basin;
  basin.Kind = EarthworkKind::Basin;
  basin.RingEastNorthM = {-10, -10, 10, -10, 10, 10, -10, 10};
  basin.LowE = basin.LowN = -10;
  basin.HighE = basin.HighN = 10;
  basin.PlateauM = -2;
  EarthworkStamp clearance;
  clearance.Kind = EarthworkKind::Clearance;
  clearance.RingEastNorthM = {-4, -1, 4, -1, 4, 1, -4, 1};
  clearance.LowE = -4;
  clearance.HighE = 4;
  clearance.LowN = -1;
  clearance.HighN = 1;
  clearance.PlateauM = 5;
  clearance.YieldM = 10;
  const std::array<EastNorth, 4> points{{{.EastM = 0, .NorthM = 0},
                                         {.EastM = 8, .NorthM = 0},
                                         {.EastM = 0, .NorthM = 5},
                                         {.EastM = 15, .NorthM = 0}}};
  for (const bool reverse : {false, true}) {
    std::array<EarthworkStamp, 2> stamps{basin, clearance};
    if (reverse) { std::ranges::reverse(stamps); }
    std::array<double, 4> heights{2, 2, 2, 2};
    const auto result = ApplyEarthworkStamps(stamps, points, heights, 80.0);
    const std::array<double, 4> expected{-2, -2, -2, 2};
    CHECK(heights == expected && result.Moved == 3,
          "a clearance ceiling cannot reserve the basin floor in either source order");
  }
  for (const bool fills : {false, true}) {
    clearance.Fills = fills;
    const std::array<EarthworkStamp, 1> stamps{clearance};
    std::array<double, 4> high{8, 8, 8, 8};
    (void)ApplyEarthworkStamps(stamps, points, high, 80.0);
    const std::array<double, 4> cut{5, 8, 8, 8};
    CHECK(high == cut, "clearance removes terrain above its ceiling only within coverage");
    std::array<double, 4> low{2, 2, 2, 2};
    (void)ApplyEarthworkStamps(stamps, points, low, 80.0);
    const std::array<double, 4> unchanged{2, 2, 2, 2};
    CHECK(low == unchanged, "a clearance volume never adds soil beneath a span");
  }
  EarthworkStamp support = clearance;
  support.Kind = EarthworkKind::Pad;
  support.PlateauM = 1;
  support.Fills = true;
  const std::array<EarthworkStamp, 3> supported{basin, clearance, support};
  std::array<double, 4> heights{2, 2, 2, 2};
  (void)ApplyEarthworkStamps(supported, points, heights, 80.0);
  const std::array<double, 4> expected{1, -2, -2, 2};
  CHECK(heights == expected, "an actual support retains its independent ground contact");
  return Report();
}
