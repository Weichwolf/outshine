#ifndef OUTSHINE_CONTENT_SHADE_UVTRANSFORM_H
#define OUTSHINE_CONTENT_SHADE_UVTRANSFORM_H

#include "math/Vec2.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace outshine {

struct UvPoint {
  double U = 0.0;
  double V = 0.0;
};

enum class UvSet : uint8_t { First, Second };

constexpr int kUvSets = 2;

struct UvTransform {
  std::array<double, 6> M = {{1.0, 0.0, 0.0, 0.0, 1.0, 0.0}};

  [[nodiscard]] UvPoint Apply(UvPoint uv) const {
    return UvPoint{.U = M[0] * uv.U + M[1] * uv.V + M[2], .V = M[3] * uv.U + M[4] * uv.V + M[5]};
  }
};

struct UvTransformProperties {
  Vec2 OffsetUv;
  double RotationRad = 0.0;
  Vec2 ScaleUv = {{1.0, 1.0}};

  [[nodiscard]] bool operator==(const UvTransformProperties &) const = default;
};

[[nodiscard]] inline UvTransform UvTransformOf(const UvTransformProperties &declared) {
  const double turn = std::cos(declared.RotationRad);
  const double lift = std::sin(declared.RotationRad);
  UvTransform composed;
  composed.M[0] = declared.ScaleUv[0] * turn;
  composed.M[1] = declared.ScaleUv[1] * lift;
  composed.M[2] = declared.OffsetUv[0];
  composed.M[3] = -declared.ScaleUv[0] * lift;
  composed.M[4] = declared.ScaleUv[1] * turn;
  composed.M[5] = declared.OffsetUv[1];
  return composed;
}

} // namespace outshine
#endif
