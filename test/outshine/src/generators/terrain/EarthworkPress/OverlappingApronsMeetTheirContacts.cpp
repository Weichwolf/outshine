#include "Check.h"
#include "EarthworkPress.h"

#include <array>
#include <cmath>
#include <vector>

namespace {

outshine::EarthworkStamp Contact(double lowE, double highE, double lowN, double bedM) {
  outshine::EarthworkStamp stamp;
  stamp.RingEastNorthM = {lowE, lowN, highE, lowN, highE, lowN + 4, lowE, lowN + 4};
  stamp.LowE = lowE;
  stamp.HighE = highE;
  stamp.LowN = lowN;
  stamp.HighN = lowN + 4;
  stamp.PlateauM = bedM;
  stamp.ApronM = 20;
  stamp.YieldM = 4;
  stamp.Fills = true;
  stamp.Kind = outshine::EarthworkKind::Corridor;
  stamp.Profile = outshine::ProfiledCorridorSpan{
      .CorridorKey = lowN < 0 ? 1u : 2u,
      .BeginM = {.EastM = lowE, .NorthM = lowN + 2},
      .EndM = {.EastM = highE, .NorthM = lowN + 2},
      .BeginDerivativeM = {.EastM = highE - lowE, .NorthM = 0, .UpM = 0},
      .EndDerivativeM = {.EastM = highE - lowE, .NorthM = 0, .UpM = 0},
      .StationLengthM = highE - lowE,
      .BeginBedM = bedM,
      .EndBedM = bedM,
      .BeginPavementHalfWidthM = 2,
      .EndPavementHalfWidthM = 2,
      .BeginHalfWidthM = 2,
      .EndHalfWidthM = 2};
  return stamp;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double stepM = 0.001;
  const std::array contacts{
      Contact(-50, 0, -2, 0), Contact(0, 50, -2, 0), Contact(-50, 50, 10, -6)};
  std::vector<EastNorth> points;
  for (int sample = -10; sample <= 10; ++sample) {
    points.push_back({.EastM = 0, .NorthM = 2 + static_cast<double>(sample) * stepM});
  }
  std::vector<double> heights(points.size(), -4);
  const auto result = ApplyEarthworkStamps(contacts, points, heights, kMostEarthworkM);
  CHECK(result.Structures == 0, "all contact heights fit the earthwork budget");
  for (size_t sample = 0; sample < heights.size(); ++sample) {
    CHECK(std::abs(heights[sample]) < 0.00001,
          "overlapping cut and fill aprons meet the pavement without a boundary step");
    if (sample != 0) {
      CHECK(std::abs(heights[sample] - heights[sample - 1]) / stepM < 0.001,
            "the shared pavement boundary has no sudden slope change");
    }
  }
  const std::array<EastNorth, 2> seams{
      {{.EastM = -1.e-12, .NorthM = 0}, {.EastM = 1.e-12, .NorthM = 0}}};
  std::array<double, 2> seamHeights{-4, -4};
  (void)ApplyEarthworkStamps(contacts, seams, seamHeights, kMostEarthworkM);
  CHECK(std::abs(seamHeights[0]) < 0.00001 && std::abs(seamHeights[1]) < 0.00001,
        "arbitrarily close shared-span samples retain the pavement contact height");
  return Report();
}
