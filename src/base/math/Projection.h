#ifndef OUTSHINE_BASE_MATH_PROJECTION_H
#define OUTSHINE_BASE_MATH_PROJECTION_H

#include <cmath>
#include <numbers>
#include "math/Mat4.h"

namespace outshine {
struct PerspectiveProjection {
  double VerticalFovRad;
  double NearM;
  double FarM;
};

struct OrthographicProjection {
  double HalfWidthM;
  double HalfHeightM;
  double NearM;
  double FarM;
};

[[nodiscard]] inline bool
ProjectionMatrix(const PerspectiveProjection &lens, double viewportAspect, Mat4 &out) {
  out.Column.fill(0);
  if (!(viewportAspect > 0) || !std::isfinite(viewportAspect) || !(lens.VerticalFovRad > 0) ||
      lens.VerticalFovRad >= std::numbers::pi || !(lens.NearM > 0) || !std::isfinite(lens.NearM) ||
      std::isnan(lens.FarM) || lens.FarM < 0) {
    return false;
  }
  const double cotangent = 1.0 / std::tan(0.5 * lens.VerticalFovRad);
  out[0] = cotangent / viewportAspect;
  out[5] = cotangent;
  out[11] = -1;
  if (lens.FarM > 0 && std::isfinite(lens.FarM)) {
    if (!(lens.FarM > lens.NearM)) { return false; }
    const double depth = lens.FarM - lens.NearM;
    out[10] = -1 - 2 * (lens.NearM / depth);
    out[14] = -2 * (lens.NearM * (lens.FarM / depth));
  } else {
    out[10] = -1;
    out[14] = -2 * lens.NearM;
  }
  return true;
}

[[nodiscard]] inline bool ProjectionMatrix(const OrthographicProjection &lens, Mat4 &out) {
  out.Column.fill(0);
  if (!(lens.HalfWidthM > 0) || !std::isfinite(lens.HalfWidthM) || !(lens.HalfHeightM > 0) ||
      !std::isfinite(lens.HalfHeightM) || !std::isfinite(lens.NearM) || !std::isfinite(lens.FarM) ||
      !(lens.FarM > lens.NearM)) {
    return false;
  }
  out[0] = 1.0 / lens.HalfWidthM;
  out[5] = 1.0 / lens.HalfHeightM;
  out[10] = 2.0 / (lens.NearM - lens.FarM);
  out[14] = (lens.FarM + lens.NearM) / (lens.NearM - lens.FarM);
  out[15] = 1;
  return true;
}
}
#endif
