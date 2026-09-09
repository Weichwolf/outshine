#ifndef OUTSHINE_UVTRANSFORM_H
#define OUTSHINE_UVTRANSFORM_H

#include "math/Vec2.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace outshine {

/// Normalized texture coordinates, with (0,0) at the image's upper-left corner.
/// U increases rightward and V downward. Coordinates may lie outside [0,1]; the
/// sampler supplies addressing. Value-only data; synchronize concurrent mutation.
struct UvPoint {
  double U = 0.0; ///< Horizontal coordinate in texture-width units.
  double V = 0.0; ///< Vertical coordinate in texture-height units.
};

/// Selects one of the geometry's independent vertex UV attributes.
enum class UvSet : uint8_t {
  Uv0, ///< Primary UV attribute.
  Uv1  ///< Secondary UV attribute.
};

/// Number of supported vertex UV attributes.
inline constexpr int kUvSets = 2;

/// Affine UV mapping stored as two row-major rows of a homogeneous 3x3 matrix.
/// The implicit final row is (0,0,1). Value-only data, with no allocation or ownership.
struct UvTransform {
  /// Coefficients (m00,m01,tx,m10,m11,ty); identity by default.
  std::array<double, 6> M = {{1.0, 0.0, 0.0, 0.0, 1.0, 0.0}};

  /// Apply the affine mapping without clamping or wrapping; constant time, no allocation.
  /// @param uv Input coordinates; caller supplies finite values and coefficients.
  /// @return Transformed coordinates; floating-point overflow is not clamped or reported.
  [[nodiscard]] constexpr UvPoint Apply(UvPoint uv) const noexcept {
    return UvPoint{.U = M[0] * uv.U + M[1] * uv.V + M[2], .V = M[3] * uv.U + M[4] * uv.V + M[5]};
  }
};

/// Declarative UV mapping: componentwise scale, rotation about (0,0), then translation.
/// Values are independent of any asset format. Zero scale collapses an axis; negative
/// scale reflects it. Callers provide finite parameters; arithmetic is not range-clamped.
struct UvTransformProperties {
  Vec2 OffsetUv; ///< Translation in normalized texture coordinates, after rotation.
  /// Angle in radians: positive rotation maps +U toward +V (clockwise on the image).
  /// Algebraically, (u,v) maps to (cos(a)u-sin(a)v, sin(a)u+cos(a)v).
  double RotationRad = 0.0;
  /// Dimensionless axis scales applied before rotation.
  Vec2 ScaleUv = {{1.0, 1.0}};

  /// @return Exact componentwise equality of the declared parameters.
  [[nodiscard]] constexpr bool operator==(const UvTransformProperties &) const noexcept = default;
};

/// Compose scale, rotation and translation into a row-major affine mapping.
/// @param declared Finite mapping parameters; caller validates external input.
/// @return Prepared mapping; constant work, no allocation, no wrapping or error recovery.
[[nodiscard]] inline UvTransform UvTransformOf(const UvTransformProperties &declared) noexcept {
  const double turn = std::cos(declared.RotationRad);
  const double lift = std::sin(declared.RotationRad);
  UvTransform composed;
  composed.M[0] = declared.ScaleUv[0] * turn;
  composed.M[1] = -declared.ScaleUv[1] * lift;
  composed.M[2] = declared.OffsetUv[0];
  composed.M[3] = declared.ScaleUv[0] * lift;
  composed.M[4] = declared.ScaleUv[1] * turn;
  composed.M[5] = declared.OffsetUv[1];
  return composed;
}

}
#endif
