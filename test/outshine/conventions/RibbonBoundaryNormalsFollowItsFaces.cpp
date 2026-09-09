#include <cmath>
#include "../../../src/base/curve/Ribbon.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (double heading : {0.0, 0.7, -1.2}) {
    ReferenceLine line;
    std::string error;
    CHECK(line.Lay({.HeadingRad = heading}, {{.LengthM = 20}}, error), "straight road frame");
    CHECK(line.Rise({{.AlongM = 0, .Value = 0, .RatePerM = 0.25},
                     {.AlongM = 20, .Value = 5, .RatePerM = 0.25}},
                    error),
          "constant grade");
    CHECK(line.Bank({{.AlongM = 0, .Value = 0.12}, {.AlongM = 20, .Value = 0.12}}, error),
          "constant bank angle");
    const auto ribbon = Sweep(line, {.HalfWidthM = 3, .ShoulderM = 1, .ThicknessM = 0.5}, 0, 20, 2);
    CHECK(ribbon.Woven, "closed graded road shell");
    const auto unshouldered = Sweep(line, {.HalfWidthM = 3, .ThicknessM = 0.5}, 0, 20, 2);
    CHECK(unshouldered.Woven, "zero shoulder remains a supported section");
    for (float normal : unshouldered.NormalM) {
      CHECK(std::isfinite(normal), "coincident shoulder vertices cannot produce NaN normals");
    }
    Vec3 centre;
    for (size_t vertex = 0; vertex < ribbon.Vertices; ++vertex) {
      for (size_t axis = 0; axis < 3; ++axis) {
        centre[axis] += ribbon.PositionM[vertex * 3 + axis] / static_cast<double>(ribbon.Vertices);
      }
    }
    for (size_t face = 0; face + 2 < ribbon.Index.size(); face += 3) {
      const auto point = [&](uint32_t vertex) {
        const size_t first = static_cast<size_t>(vertex) * 3;
        return Vec3{
            {ribbon.PositionM[first], ribbon.PositionM[first + 1], ribbon.PositionM[first + 2]}};
      };
      const auto a = point(ribbon.Index[face]);
      const auto b = point(ribbon.Index[face + 1]);
      const auto c = point(ribbon.Index[face + 2]);
      Vec3 ab;
      Vec3 ac;
      for (size_t axis = 0; axis < 3; ++axis) {
        ab[axis] = b[axis] - a[axis];
        ac[axis] = c[axis] - a[axis];
      }
      const auto faceNormal = Cross(ab, ac);
      const double area = std::hypot(faceNormal[0], faceNormal[1], faceNormal[2]);
      double outward = 0;
      for (size_t axis = 0; axis < 3; ++axis) {
        outward += faceNormal[axis] * (a[axis] - centre[axis]);
      }
      CHECK(outward > 0, "convex shell triangles face away from its interior");
      CHECK(area > 0, "every emitted triangle has positive area");
      for (size_t corner = 0; corner < 3; ++corner) {
        const size_t at = static_cast<size_t>(ribbon.Index[face + corner]) * 3;
        double alignment = 0;
        double norm = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
          const double value = ribbon.NormalM[at + axis];
          alignment += value * faceNormal[axis] / area;
          norm += value * value;
        }
        CHECK_NEAR(norm, 1, 1e-6, "unit", "all shell normals are unit vectors");
        CHECK_NEAR(alignment,
                   1,
                   1e-6,
                   "unit",
                   "planar deck, soffit, walls and caps match their outward triangle normal");
      }
    }
  }
  return Report();
}
