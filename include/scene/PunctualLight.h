#ifndef OUTSHINE_PUNCTUALLIGHT_H
#define OUTSHINE_PUNCTUALLIGHT_H

#include "math/Vec3.h"
#include <numbers>

namespace outshine {

/// A spot's outer half-angle: pi/4 radians, giving a full cone angle of pi/2.
constexpr float kOuterConeUnsaidRad = std::numbers::pi_v<float> / 4.0f;

enum class LightKind { Directional, Point, Spot };

struct PunctualLight {
  LightKind Kind = LightKind::Directional;
  Vec3f Colour = {{1.0f, 1.0f, 1.0f}};
  /// Illuminance in lux for directional lights; luminous intensity in candela for point/spot.
  float Intensity = 1.0f;
  /// Position in the light's local coordinate frame, in metres.
  Vec3f Position;
  /// Local direction of emitted rays; placement orients it without scaling intensity or range.
  Vec3f Direction = {{0.0f, 0.0f, -1.0f}};

  float InnerConeRad = 0.0f;
  float OuterConeRad = kOuterConeUnsaidRad;
  /// Distance cutoff for point/spot lights; zero means unbounded.
  float RangeM = 0.0f;
};

}
#endif
