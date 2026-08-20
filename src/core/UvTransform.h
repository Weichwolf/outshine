#ifndef UVTRANSFORM_H
#define UVTRANSFORM_H

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
  double M[6] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};

  [[nodiscard]] UvPoint Apply(UvPoint uv) const {
    return UvPoint{M[0] * uv.U + M[1] * uv.V + M[2], M[3] * uv.U + M[4] * uv.V + M[5]};
  }
};

struct UvTransformProperties {
  double OffsetUv[2] = {0.0, 0.0};
  double RotationRad = 0.0;
  double ScaleUv[2] = {1.0, 1.0};
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

}
#endif
