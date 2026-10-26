#include "CloudLayer.h"
#include "Check.h"

#include <cmath>

namespace {
constexpr double kEyeM = 1772.0;
constexpr double kBaseM = 2572.0;
constexpr double kTopM = 3172.0;
constexpr double kInsideM = 2800.0;
constexpr double kAboveM = 4000.0;
constexpr double kGeometryToleranceM = 1e-8;
constexpr double kHorizonToleranceM = 1e-7;
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const CloudLayer layer{.BaseM = kBaseM, .Cover = 1.0};
  const Vec3 below{{0.0, kEyeM, 0.0}};
  const Vec3 up{{0.0, 1.0, 0.0}};
  const auto upward = CloudShell(layer, below, up);
  CHECK(std::abs(upward[0].BeginM - (kBaseM - kEyeM)) < kGeometryToleranceM &&
            std::abs(upward[0].EndM - (kTopM - kEyeM)) < kGeometryToleranceM,
        "an upward ray crosses the declared 600-m layer at exact height offsets");
  const auto downward = CloudShell(layer, below, up * -1.0);
  CHECK(downward[0].EndM == 0.0 && downward[1].EndM == 0.0,
        "the opaque planet hides the far-side layer");
  const auto inside = CloudShell(layer, {{0.0, kInsideM, 0.0}}, up);
  CHECK(inside[0].BeginM == 0.0 &&
            std::abs(inside[0].EndM - (kTopM - kInsideM)) < kGeometryToleranceM,
        "an observer inside the layer marches only to the exit");
  const auto above = CloudShell(layer, {{0.0, kAboveM, 0.0}}, up * -1.0);
  CHECK(std::abs(above[0].BeginM - (kAboveM - kTopM)) < kGeometryToleranceM &&
            std::abs(above[0].EndM - (kAboveM - kBaseM)) < kGeometryToleranceM,
        "a downward ray from above begins at the cloud top and ends at its base");
  const auto horizon = CloudShell(layer, below, {{1.0, 0.0, 0.0}});
  const double eyeRadius = layer.RadiusM + kEyeM;
  const double topRadius = layer.RadiusM + kTopM;
  const double baseRadius = layer.RadiusM + kBaseM;
  const double exactSpan = std::sqrt(topRadius * topRadius - eyeRadius * eyeRadius) -
                           std::sqrt(baseRadius * baseRadius - eyeRadius * eyeRadius);
  CHECK(std::abs(horizon[0].EndM - horizon[0].BeginM - exactSpan) < kHorizonToleranceM,
        "a tangent ray has a finite curved-shell span without a horizon denominator clamp");
  CHECK(CloudAltitude({{0.0, kEyeM, 0.0}}, layer.RadiusM) == kEyeM,
        "relative altitude retains the declared metre-scale height");
  const double tangentX = (layer.RadiusM + 2400.0) / (layer.RadiusM + kAboveM);
  const Vec3 across{{tangentX, -std::sqrt(1.0 - tangentX * tangentX), 0.0}};
  const auto twoSides = CloudShell(layer, {{0.0, kAboveM, 0.0}}, across);
  CHECK(twoSides[0].EndM > twoSides[0].BeginM && twoSides[1].EndM > twoSides[1].BeginM &&
            twoSides[1].BeginM > twoSides[0].EndM,
        "a grazing ray above the planet crosses both separated visible shell intervals");
  const auto surfaceTangent = CloudShell(layer, {{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}});
  CHECK(surfaceTangent[0].EndM > surfaceTangent[0].BeginM,
        "a ray tangent at the ground is not mistaken for an inward ground hit");
  return Report();
}
