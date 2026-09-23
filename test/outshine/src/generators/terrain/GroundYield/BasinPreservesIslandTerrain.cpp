#include "GroundYield.h"

#include "Check.h"

#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  EarthworkStamp basin;
  basin.Kind = EarthworkKind::Basin;
  basin.RingEastNorthM = {0, 0, 10, 0, 10, 10, 0, 10};
  basin.HoleRingsEastNorthM = {{3, 3, 3, 7, 7, 7, 7, 3}};
  basin.LowE = basin.LowN = 0;
  basin.HighE = basin.HighN = 10;
  basin.PlateauM = -2;
  const std::array<EastNorth, 3> points{
      {{.EastM = 2, .NorthM = 5}, {.EastM = 5, .NorthM = 5}, {.EastM = 11, .NorthM = 5}}};
  std::array<double, 3> heights{};
  const std::array<EarthworkStamp, 1> withIsland{basin};
  const Pressed accepted = PressPoints(withIsland, points, heights, 30.0);
  const std::array<double, 3> preserved{-2, 0, 0};
  CHECK(heights == preserved && accepted.Moved == 1,
        "basin cuts open water while island and outside terrain remain untouched");

  basin.HoleRingsEastNorthM.clear();
  heights.fill(0);
  const std::array<EarthworkStamp, 1> withoutIsland{basin};
  const Pressed filled = PressPoints(withoutIsland, points, heights, 30.0);
  const std::array<double, 3> filledExpected{-2, -2, 0};
  CHECK(heights == filledExpected && filled.Moved == 2,
        "removing the hole makes its former centre part of the basin");
  return Report();
}
