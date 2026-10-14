#include "Check.h"
#include "RoofSurface.h"

#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

double Area(std::span<const EastNorth> triangles) {
  double result = 0;
  for (size_t at = 0; at < triangles.size(); at += 3) {
    const auto a = triangles[at];
    const auto b = triangles[at + 1];
    const auto c = triangles[at + 2];
    const double cross =
        (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
    CHECK(cross > 0, "roof triangles have positive area and outward winding");
    result += cross / 2;
  }
  return result;
}

void Coherent(const RoofSurface &roof, std::span<const EastNorth> triangles) {
  std::vector<double> scratch;
  for (size_t at = 0; at < triangles.size(); at += 3) {
    std::array<Vec3, 3> points{};
    EastNorth centre{};
    double meanHeight = 0;
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto p = triangles[at + corner];
      const double height = roof.HeightAt(p);
      points[corner] = {{p.EastM, p.NorthM, height}};
      centre.EastM += p.EastM / 3;
      centre.NorthM += p.NorthM / 3;
      meanHeight += height / 3;
    }
    const auto ray = roof.Trace({{centre.EastM, centre.NorthM, 20}}, {{0, 0, -1}}, 0, 30, scratch);
    CHECK(ray.has_value(), "every emitted native roof triangle is ray-visible from above");
    if (!ray) { continue; }
    CHECK_NEAR(
        20 - ray->Along, meanHeight, 1e-9, "roof metres", "native ray and mesh use the same plane");
    Vec3 normal = Cross(points[1] - points[0], points[2] - points[0]);
    normal = normal * (1 / std::hypot(normal[0], normal[1], normal[2]));
    CHECK_NEAR(Dot(normal, ray->Normal),
               1,
               1e-9,
               "normal cosine",
               "ray normal matches geometric mesh normal");
  }
}

BuildingShape Triangle() {
  BuildingShape shape;
  shape.Ring = {{0, 0}, {12, 0}, {0, 9}};
  shape.Centre = {6, 4.5};
  shape.AxisU = {1, 0};
  shape.HalfUm = 6;
  shape.HalfVm = 4.5;
  shape.AreaM2 = 54;
  shape.Roof = RoofKind::Hip;
  shape.RiseM = 3;
  return shape;
}

void TriangleAndClip() {
  const auto shape = Triangle();
  const RoofSurface roof(shape);
  BuildingScratch scratch;
  std::vector<EastNorth> triangles;
  CHECK(roof.Cover(shape.Ring, scratch, triangles),
        "actual triangular footprint has complete roof coverage");
  CHECK_NEAR(Area(triangles), 54, 1e-9, "square metres", "roof preserves original footprint area");
  Coherent(roof, triangles);
  for (const auto p : shape.Ring) {
    CHECK_NEAR(roof.HeightAt(p), 0, 1e-9, "roof metres", "all actual eaves stay level");
  }
  const std::array<EastNorth, 4> clip{{{0, 0}, {4, 0}, {4, 6}, {0, 6}}};
  triangles.clear();
  CHECK(roof.Cover(clip, scratch, triangles),
        "requested roof subregion is clipped to native facets");
  CHECK_NEAR(Area(triangles),
             24,
             1e-9,
             "square metres",
             "clipped roof does not expand to entire source roof");
  Coherent(roof, triangles);
  const auto before = triangles;
  CHECK(!roof.Cover(std::span(clip).first(2), scratch, triangles),
        "invalid roof clipping fails explicitly");
  CHECK(triangles == before, "failed coverage preserves previously emitted triangles");
}

void CourtyardAndPreparation() {
  auto shape = Triangle();
  CHECK(RoofSurface::PrepareHip(shape).has_value(), "shared roof preparation succeeds");
  const auto prepared = shape.HipRoof;
  const double top = shape.TopM();
  CHECK_NEAR(top, 3, 1e-9, "building metres", "slope clamping retains requested overall height");
  CHECK_NEAR(
      shape.RiseM, 1.5, 1e-9, "roof metres", "rise follows actual inradius and allowed slope");
  CHECK(RoofSurface::PrepareHip(shape).has_value() && prepared == shape.HipRoof,
        "repeated preparation reuses immutable plan instead of rebuilding skeleton");
  shape.Ring = {{0, 0}, {20, 0}, {20, 20}, {0, 20}};
  shape.Holes = {{{8, 8}, {8, 12}, {12, 12}, {12, 8}}};
  shape.HipRoof.reset();
  shape.RiseM = 2;
  CHECK(RoofSurface::PrepareHip(shape).has_value(),
        "courtyard plan is prepared before immutable publication");
  const RoofSurface roof(shape);
  BuildingScratch scratch;
  std::vector<EastNorth> triangles;
  CHECK(roof.Cover(shape.Ring, scratch, triangles), "courtyard roof facets are emitted");
  CHECK_NEAR(
      Area(triangles), 384, 1e-9, "square metres", "courtyard remains open in native geometry");
  Coherent(roof, triangles);
  std::vector<double> cuts;
  CHECK(!roof.Trace({{10, 10, 20}}, {{0, 0, -1}}, 0, 30, cuts),
        "rays pass through courtyard opening");
}
}

int main() {
  TriangleAndClip();
  CourtyardAndPreparation();
  return Report();
}
