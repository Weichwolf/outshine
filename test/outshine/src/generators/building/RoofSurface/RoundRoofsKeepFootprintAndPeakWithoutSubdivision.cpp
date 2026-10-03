#include "RoofSurface.h"
#include "Check.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  for (const size_t corners : {16u, 64u}) {
    BuildingShape shape;
    shape.Roof = RoofKind::Dome;
    shape.AxisU = {.EastM = 1, .NorthM = 0};
    shape.HalfUm = 20;
    shape.HalfVm = 12;
    shape.RiseM = 6;
    for (size_t at = 0; at < corners; ++at) {
      const double angle =
          0.123 + 2 * std::numbers::pi * static_cast<double>(at) / static_cast<double>(corners);
      shape.Ring.push_back({.EastM = 20 * std::cos(angle), .NorthM = 12 * std::sin(angle)});
    }
    RoofSurface roof(shape);
    BuildingScratch scratch;
    std::vector<EastNorth> triangles;
    roof.Cover(shape.Ring, scratch, triangles);
    CHECK(!triangles.empty() && triangles.size() / 3 <= 2 * corners,
          "a simple round roof has linear cost in source footprint corners");
    double areaM2 = 0;
    double highestM = 0;
    for (size_t at = 0; at + 2 < triangles.size(); at += 3) {
      const auto &a = triangles[at];
      const auto &b = triangles[at + 1];
      const auto &c = triangles[at + 2];
      const double twiceArea =
          (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
      CHECK(twiceArea > 0, "roof triangles retain positive winding without degeneracy");
      areaM2 += twiceArea / 2;
      for (const auto &point : {a, b, c}) {
        const double heightM = roof.HeightAt(point);
        CHECK(std::isfinite(heightM) && heightM >= 0 && heightM <= 6,
              "roof vertices remain inside the original height interval");
        highestM = std::max(highestM, heightM);
      }
    }
    const double expectedAreaM2 = static_cast<double>(corners) * 20 * 12 / 2 *
                                  std::sin(2 * std::numbers::pi / static_cast<double>(corners));
    CHECK_NEAR(areaM2,
               expectedAreaM2,
               1e-6,
               "roof area m2",
               "all source footprint area remains covered without overlap");
    CHECK_NEAR(highestM,
               6,
               1e-9,
               "roof peak m",
               "simplifying a round roof retains its peak instead of flattening it");
    std::printf("%zu footprint corners: %zu roof triangles, peak %.9g m\n",
                corners,
                triangles.size() / 3,
                highestM);
  }
  return Report();
}
