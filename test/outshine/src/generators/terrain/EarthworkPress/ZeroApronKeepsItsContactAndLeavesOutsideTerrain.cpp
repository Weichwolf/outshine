#include "Check.h"
#include "EarthworkPress.h"

#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const auto kind : {EarthworkKind::Corridor, EarthworkKind::Pad, EarthworkKind::Clearance}) {
    const std::array stamps{EarthworkStamp{.RingEastNorthM = {-10, -2, 10, -2, 10, 2, -10, 2},
                                           .LowE = -10,
                                           .HighE = 10,
                                           .LowN = -2,
                                           .HighN = 2,
                                           .PlateauM = -6,
                                           .ApronM = 0,
                                           .YieldM = 6,
                                           .Fills = true,
                                           .Kind = kind}};
    const std::array<EastNorth, 3> samples{{{0, 0}, {0, 2}, {0, 2.001}}};
    std::array<double, 3> heights{};
    (void)ApplyEarthworkStamps(stamps, samples, heights, kMostEarthworkM);
    CHECK(heights[0] == -6 && heights[1] == -6,
          "a zero apron keeps the physical contact through its boundary");
    CHECK(heights[2] == 0, "a zero apron never invents an earthwork transition outside the core");
  }
  return Report();
}
