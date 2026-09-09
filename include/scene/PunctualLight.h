#ifndef OUTSHINE_PUNCTUALLIGHT_H
#define OUTSHINE_PUNCTUALLIGHT_H

#include "math/Vec3.h"
#include <numbers>

namespace outshine {

/// A spot's outer half-angle: pi/4 radians, giving a full cone angle of pi/2.
constexpr float kOuterConeUnsaidRad = std::numbers::pi_v<float> / 4.0f;

/// Idealized emitter shape; finite area emitters require a separate representation.
enum class LightKind {
  Directional, ///< Parallel incident rays; illuminance does not decay with distance.
  Point,       ///< Isotropic point emitter with inverse-square distance falloff.
  Spot         ///< Point emitter constrained by inner/outer cone half-angles.
};

/// Native photometric emitter parameters, separate from the owning geometry's placement.
/// No pointers or resource ownership; copies are independent and allocate nothing. Supply
/// finite values in the documented ranges; this aggregate does not validate its contents.
/// No thread affinity or synchronization; immutable reads may run concurrently, shared writes
/// require external synchronization. Shadow resources and volumetric injection are renderer
/// responsibilities; declaring this value alone does not establish support for those effects.
struct PunctualLight {
  /// Supported emitter shape; determines intensity units and applicable spatial fields.
  LightKind Kind = LightKind::Directional;
  /// Linear RGB colour multiplier, each component in [0,1]; intensity carries photometric scale.
  Vec3f Colour = {{1.0f, 1.0f, 1.0f}};
  /// Nonnegative illuminance in lux for directional lights; candela for point/spot emitters.
  float Intensity = 1.0f;
  /// Local emitter position in metres; ignored for directional illumination.
  Vec3f Position;
  /// Unit local direction of emitted rays for directional/spot lights; ignored for point lights.
  /// Placement transforms and normalizes the direction; it does not scale intensity or range.
  Vec3f Direction = {{0.0f, 0.0f, -1.0f}};

  /// Spot inner half-angle in radians: 0 <= InnerConeRad < OuterConeRad; full intensity inside.
  float InnerConeRad = 0.0f;
  /// Spot outer half-angle in radians: 0 < OuterConeRad <= pi/2; no spot contribution outside.
  float OuterConeRad = kOuterConeUnsaidRad;
  /// Nonnegative distance cutoff in metres for point/spot lights; zero means unbounded.
  /// Finite range attenuates smoothly toward zero; ignored for directional illumination.
  float RangeM = 0.0f;
};

}
#endif
