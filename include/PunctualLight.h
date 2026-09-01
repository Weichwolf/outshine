#ifndef OUTSHINE_PUNCTUALLIGHT_H
#define OUTSHINE_PUNCTUALLIGHT_H

#include "Vec3.h"
#include <numbers>

namespace outshine {

enum class LightKind { Directional, Point, Spot };

struct PunctualLight {
  LightKind Kind = LightKind::Directional;
  Vec3f Colour = {{1.0f, 1.0f, 1.0f}};
  float Intensity = 1.0f;
  Vec3f Position;
  Vec3f Direction = {{0.0f, 0.0f, -1.0f}};

  float InnerConeRad = 0.0f;
  float OuterConeRad = 0.25f * std::numbers::pi_v<float>;
  float RangeM = 0.0f;
};

} // namespace outshine
#endif
