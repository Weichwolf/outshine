#include "CloudLayer.h"
#include "Check.h"

#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const CloudLayer layer{.BaseM = 2572.0, .Cover = 1.0};
  const Vec3 below{{0.0, 1772.0, 0.0}};
  const Vec3 up{{0.0, 1.0, 0.0}};
  const auto upward = CloudShell(layer, below, up);
  CHECK(std::abs(upward[0].BeginM - 800.0) < 1e-8 && std::abs(upward[0].EndM - 1400.0) < 1e-8,
        "an upward ray crosses the declared 600-m layer at exact height offsets");
  const auto downward = CloudShell(layer, below, up * -1.0);
  CHECK(downward[0].EndM == 0.0 && downward[1].EndM == 0.0,
        "the opaque planet hides the far-side layer");
  const auto inside = CloudShell(layer, {{0.0, 2800.0, 0.0}}, up);
  CHECK(inside[0].BeginM == 0.0 && std::abs(inside[0].EndM - 372.0) < 1e-8,
        "an observer inside the layer marches only to the exit");
  const auto above = CloudShell(layer, {{0.0, 4000.0, 0.0}}, up * -1.0);
  CHECK(std::abs(above[0].BeginM - 828.0) < 1e-8 && std::abs(above[0].EndM - 1428.0) < 1e-8,
        "a downward ray from above begins at the cloud top and ends at its base");
  const auto horizon = CloudShell(layer, below, {{1.0, 0.0, 0.0}});
  const double eyeRadius = layer.RadiusM + 1772.0;
  const double topRadius = layer.RadiusM + 3172.0;
  const double baseRadius = layer.RadiusM + 2572.0;
  const double exactSpan = std::sqrt(topRadius * topRadius - eyeRadius * eyeRadius) -
                           std::sqrt(baseRadius * baseRadius - eyeRadius * eyeRadius);
  CHECK(std::abs(horizon[0].EndM - horizon[0].BeginM - exactSpan) < 1e-7,
        "a tangent ray has a finite curved-shell span without a horizon denominator clamp");
  CHECK(CloudAltitude({{0.0, 1772.0, 0.0}}, layer.RadiusM) == 1772.0,
        "relative altitude retains the declared metre-scale height");
  const double tangentX = (layer.RadiusM + 2400.0) / (layer.RadiusM + 4000.0);
  const Vec3 across{{tangentX, -std::sqrt(1.0 - tangentX * tangentX), 0.0}};
  const auto twoSides = CloudShell(layer, {{0.0, 4000.0, 0.0}}, across);
  CHECK(twoSides[0].EndM > twoSides[0].BeginM && twoSides[1].EndM > twoSides[1].BeginM &&
            twoSides[1].BeginM > twoSides[0].EndM,
        "a grazing ray above the planet crosses both separated visible shell intervals");
  const auto surfaceTangent = CloudShell(layer, {{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}});
  CHECK(surfaceTangent[0].EndM > surfaceTangent[0].BeginM,
        "a ray tangent at the ground is not mistaken for an inward ground hit");
  return Report();
}
