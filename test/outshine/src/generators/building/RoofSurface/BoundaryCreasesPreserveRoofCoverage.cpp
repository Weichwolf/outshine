#include "Check.h"
#include "RoofSurface.h"
#include <cstddef>
#include <vector>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;

  BuildingShape shape;
  shape.Ring = {{.EastM = 0, .NorthM = 0},
                {.EastM = 8, .NorthM = 0},
                {.EastM = 8, .NorthM = 8},
                {.EastM = 0, .NorthM = 8},
                {.EastM = 0, .NorthM = 4}};
  shape.Centre = {.EastM = 4, .NorthM = 4};
  shape.AxisU = {.EastM = 1, .NorthM = 0};
  shape.HalfUm = 4;
  shape.HalfVm = 4;
  shape.Roof = RoofKind::Sawtooth;
  shape.PeriodM = 4;
  shape.RiseM = 1;

  BuildingScratch scratch;
  std::vector<outshine::EastNorth> triangles;
  const RoofSurface roof(shape);
  CHECK(roof.Cover(shape.Ring, scratch, triangles),
        "a crease along a subdivided eave preserves the roof");
  CHECK(triangles.size() > 6, "interior creases still split the surface into facets");
  double area = 0;
  for (size_t at = 0; at + 2 < triangles.size(); at += 3) {
    const auto a = triangles[at];
    const auto b = triangles[at + 1];
    const auto c = triangles[at + 2];
    const double twiceArea =
        (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
    CHECK(twiceArea > 0, "every emitted roof triangle has positive area");
    area += twiceArea / 2;
  }
  CHECK_NEAR(area, 64, 1e-9, "square metres", "all roof area remains covered");
  return Report();
}
