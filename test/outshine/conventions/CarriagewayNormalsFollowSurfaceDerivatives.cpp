#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <export/GltfExporter.h>
#include <scene/Geometry.h>
#include "../../../src/base/curve/Carriageway.h"
#include "../../../src/base/curve/Ribbon.h"
#include "Check.h"

namespace {
outshine::Vec3 Point(double s, double t, double curvature, double slope, double bankRate) {
  const double angle = curvature * s;
  const double east = curvature == 0 ? s : std::sin(angle) / curvature;
  const double north = curvature == 0 ? 0 : (1 - std::cos(angle)) / curvature;
  return {{east - t * std::sin(angle),
           slope * s - t * std::tan(0.07 + bankRate * s),
           north + t * std::cos(angle)}};
}

bool Configure(outshine::ReferenceLine &line, double curvature, double slope, double bankRate) {
  using namespace outshine;
  std::string error;
  return line.Lay({},
                  {{.Shape = curvature == 0 ? Curve::Straight : Curve::Arc,
                    .LengthM = 50,
                    .EntryCurvature = curvature,
                    .ExitCurvature = curvature}},
                  error) &&
         line.Rise({{.AlongM = 0, .Value = 0, .RatePerM = slope},
                    {.AlongM = 50, .Value = 50 * slope, .RatePerM = slope}},
                   error) &&
         line.Bank({{.AlongM = 0, .Value = 0.07, .RatePerM = bankRate},
                    {.AlongM = 50, .Value = 0.07 + 50 * bankRate, .RatePerM = bankRate}},
                   error);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double delta = 1e-4;
  for (double curvature : {-0.02, 0.0, 0.02}) {
    for (double slope : {-0.16, 0.0, 0.16}) {
      for (double bankRate : {-0.003, 0.0, 0.003}) {
        ReferenceLine line;
        CHECK(Configure(line, curvature, slope, bankRate), "analytic reference line is accepted");
        for (double s : {5.0, 20.0, 45.0}) {
          for (double t : {-4.0, 0.0, 4.0}) {
            const auto surface = StandAt(line, {.AlongM = s, .AcrossM = t, .HalfWidthM = 4});
            const auto before = Point(s - delta, t, curvature, slope, bankRate);
            const auto after = Point(s + delta, t, curvature, slope, bankRate);
            const auto left = Point(s, t - delta, curvature, slope, bankRate);
            const auto right = Point(s, t + delta, curvature, slope, bankRate);
            double alongDot = 0;
            double acrossDot = 0;
            double lengthSquared = 0;
            for (size_t axis = 0; axis < 3; ++axis) {
              alongDot += surface.NormalM[axis] * (after[axis] - before[axis]) / (2 * delta);
              acrossDot += surface.NormalM[axis] * (right[axis] - left[axis]) / (2 * delta);
              lengthSquared += surface.NormalM[axis] * surface.NormalM[axis];
            }
            CHECK(surface.On && surface.NormalM[1] > 0,
                  "surface is present with upward orientation");
            CHECK_NEAR(surface.HeightM,
                       Point(s, t, curvature, slope, bankRate)[1],
                       1e-12,
                       "m",
                       "height follows the declared offset surface");
            CHECK_NEAR(alongDot,
                       0,
                       1e-8,
                       "unit",
                       "normal is perpendicular to the offset longitudinal tangent");
            CHECK_NEAR(
                acrossDot, 0, 1e-8, "unit", "normal is perpendicular to the transverse tangent");
            CHECK_NEAR(lengthSquared, 1, 1e-12, "unit", "surface normal is unit length");
          }
        }
      }
    }
  }
  if (const char *path = std::getenv("OUTSHINE_TEST_GLB")) {
    ReferenceLine line;
    CHECK(Configure(line, 0.02, 0.16, 0.003), "render fixture uses the tested curve family");
    const auto ribbon =
        Sweep(line, {.HalfWidthM = 4, .ShoulderM = 0.5, .ThicknessM = 0.4}, 0, 50, 0.5);
    CHECK(ribbon.Woven, "production sweep generates render fixture");
    Geometry geometry;
    Material material;
    material.BaseColour = {0.35f, 0.35f, 0.35f, 1};
    material.Roughness = 0.35f;
    const int part = geometry.addPart("graded curved road", geometry.addSurface("road", material));
    CHECK(geometry.setPositions(part, ribbon.PositionM) &&
              geometry.setNormals(part, ribbon.NormalM) &&
              geometry.setTriangles(part, ribbon.Index),
          "native geometry receives actual sweep output");
    auto glb = exportGlb(geometry);
    CHECK(glb.has_value(), "public exporter accepts native render fixture");
    if (glb) {
      std::ofstream file(path, std::ios::binary);
      file.write(reinterpret_cast<const char *>(glb->data()),
                 static_cast<std::streamsize>(glb->size()));
      CHECK(file.good(), "render fixture is written completely");
    }
  }
  return Report();
}
