#include <world/data/GeoCellId.h>
#include <Earth.h>
#include "Check.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto includes = [](const auto &cells, double latitude, double longitude) {
    return std::ranges::any_of(cells, [&](GeoCellId cell) {
      const auto bounds = cell.Bounds();
      return bounds && latitude >= bounds->SouthDeg && latitude <= bounds->NorthDeg &&
             longitude >= bounds->WestDeg && longitude <= bounds->EastDeg;
    });
  };
  constexpr double radius = 240000.0;
  const auto equator = CellsAround(0.0, 0.0, radius, 9, 262144);
  CHECK(equator && includes(*equator, 0.0, 0.0), "origin remains in full source demand");
  if (equator) {
    CHECK(includes(*equator, 2.13, -2.13) && includes(*equator, 2.13, 2.13),
          "the source query envelope retains outer nodes of a way crossing the requested disc");
    const double equatorialEnd = radius / kWgs84A * kRad2Deg;
    CHECK(includes(*equator, 0.0, equatorialEnd) && includes(*equator, 0.0, -equatorialEnd),
          "analytic WGS84 equatorial geodesic endpoints remain covered");
    CHECK(!CellsAround(0.0, 0.0, radius, 9, equator->size() - 1),
          "insufficient admission fails instead of shortening coverage");
  }
  const auto wrapped = CellsAround(20.0, 179.9, radius, 9, 262144);
  CHECK(wrapped && includes(*wrapped, 20.0, -179.9) && includes(*wrapped, 20.0, 179.9),
        "dateline keeps both geographic halves");
  if (wrapped) {
    auto sorted = *wrapped;
    std::ranges::sort(sorted,
                      [](auto a, auto b) { return a.X < b.X || (a.X == b.X && a.Y < b.Y); });
    CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end(),
          "wrapped source demand has unique cells");
  }
  const auto polar = CellsAround(89.9, 0.0, radius, 9, 262144);
  CHECK(polar && includes(*polar, 90.0, -180.0) && includes(*polar, 90.0, 180.0),
        "a disc reaching the pole keeps every longitude");
  const auto globe = CellsAround(0.0, 180.0, std::numbers::pi * kWgs84A, 4, 256);
  CHECK(globe && globe->size() == 256, "full Earth has every cell exactly once");
  const auto coarse = CellsAround(0.0, 180.0, 0.0, 0, 1);
  CHECK(coarse && coarse->size() == 1, "coarsest dateline demand does not duplicate the root");
  for (const double invalid :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    CHECK(!CellsAround(0.0, 0.0, invalid, 9, 262144), "invalid radius has no demand");
  }
  CHECK(!CellsAround(91.0, 0.0, radius, 9, 262144) && !CellsAround(0.0, 181.0, radius, 9, 262144) &&
            !CellsAround(0.0, 0.0, radius, 25, 262144) && !CellsAround(0.0, 0.0, radius, 9, 0),
        "invalid coordinates, level or capacity are rejected");
  return Report();
}
