#ifndef OUTSHINE_PUNCTUALLIGHT_H
#define OUTSHINE_PUNCTUALLIGHT_H

#include <numbers>

namespace outshine {

enum class LightKind { Directional, Point, Spot };

struct PunctualLight {
  LightKind Kind = LightKind::Directional;
  float Colour[3] = {1.0f, 1.0f, 1.0f};
  float Intensity = 1.0f;
  float Position[3] = {0.0f, 0.0f, 0.0f};
  float Direction[3] = {0.0f, 0.0f, -1.0f};

  float InnerConeRad = 0.0f;
  float OuterConeRad = 0.25f * std::numbers::pi_v<float>;
  float RangeM = 0.0f;
};

} // namespace outshine
#endif
