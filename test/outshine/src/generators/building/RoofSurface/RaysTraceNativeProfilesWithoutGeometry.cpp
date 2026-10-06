#include "RoofSurface.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  BuildingShape shape;
  shape.Centre = {.EastM = 13, .NorthM = -7};
  shape.AxisU = {.EastM = 0.6, .NorthM = 0.8};
  shape.HalfUm = 30;
  shape.HalfVm = 12;
  shape.RiseM = 6;
  shape.BreakFracV = 0.4;
  shape.BreakRiseM = 4;
  shape.PeriodM = 3;
  for (const auto &point : std::array<Boxed, 4>{{{-30, -12}, {30, -12}, {30, 12}, {-30, 12}}}) {
    shape.Ring.push_back(shape.FromBox(point));
  }
  BuildingScratch scratch;
  for (const auto kind : {RoofKind::Flat,
                          RoofKind::Gable,
                          RoofKind::Hip,
                          RoofKind::Shed,
                          RoofKind::Mansard,
                          RoofKind::Sawtooth,
                          RoofKind::Dome}) {
    shape.Roof = kind;
    const RoofSurface roof(shape);
    for (int x = -29; x <= 29; x += 2) {
      for (int y = -11; y <= 11; y += 2) {
        const EastNorth point = shape.FromBox({.U = x + 0.123, .V = y + 0.234});
        const auto hit =
            roof.Trace({{point.EastM, point.NorthM, 20}}, {{0, 0, -2}}, 0, 20, scratch.At);
        CHECK(hit.has_value(), "every native roof profile is directly intersected");
        if (!hit) { continue; }
        CHECK_NEAR(20 - 2 * hit->Along,
                   roof.HeightAt(point),
                   1e-9,
                   "roof height m",
                   "the query agrees with the native profile without roof triangulation");
        CHECK_NEAR(std::hypot(hit->Normal[0], hit->Normal[1], hit->Normal[2]),
                   1,
                   1e-12,
                   "normal length",
                   "native roof lighting gets a unit outward normal");
        CHECK(hit->Normal[2] > 0, "a roof normal points above its surface");
        const auto clipped = roof.Trace(
            {{point.EastM, point.NorthM, 20}}, {{0, 0, -2}}, 0, hit->Along - 0.01, scratch.At);
        CHECK(!clipped, "a nearer visibility bound avoids hidden roof hits");
      }
    }
  }
  shape.Roof = RoofKind::Sawtooth;
  const RoofSurface saw(shape);
  const EastNorth left = shape.FromBox({.U = -35, .V = 0});
  const Vec3 alongU{{shape.AxisU.EastM, shape.AxisU.NorthM, 0}};
  const auto first = saw.Trace({{left.EastM, left.NorthM, 3}}, alongU, 0, 100, scratch.At);
  CHECK(first.has_value(), "a horizontal ray crosses a native sawtooth slope");
  if (first) {
    CHECK_NEAR(first->Along,
               5 + 0.5 * 0.85 * shape.PeriodM,
               1e-9,
               "nearest ridge m",
               "ridge crossings are ordered from the query origin");
  }
  const auto distant = saw.Trace({{left.EastM, left.NorthM, 3}}, alongU, 56, 100, scratch.At);
  CHECK(distant.has_value(), "later roof ridges survive beyond the mesher's crease limit");
  if (distant) {
    CHECK_NEAR(distant->Along,
               56 + 0.5 * 0.85 * shape.PeriodM,
               1e-9,
               "later ridge m",
               "source-ray evaluation is not limited to seven roof periods");
  }
  shape.Holes.emplace_back();
  for (const auto &point : std::array<Boxed, 4>{{{-2, -2}, {2, -2}, {2, 2}, {-2, 2}}}) {
    shape.Holes.back().push_back(shape.FromBox(point));
  }
  shape.Roof = RoofKind::Gable;
  const RoofSurface courtyard(shape);
  CHECK(!courtyard.Trace({{13, -7, 20}}, {{0, 0, -1}}, 0, 100, scratch.At),
        "a source courtyard remains open in a direct roof query");
  CHECK(!courtyard.Trace({{13, -7, 20}}, {}, 0, 100, scratch.At), "zero direction misses");
  CHECK(!courtyard.Trace(
            {{std::numeric_limits<double>::quiet_NaN(), 0, 20}}, {{0, 0, -1}}, 0, 100, scratch.At),
        "invalid rays cannot generate surfaces");
  CHECK(scratch.Tris.empty() && scratch.Cells.Count() == 0 && scratch.Welded.Size() == 0,
        "native roof queries allocate no source vertices or triangle cells");
  return Report();
}
