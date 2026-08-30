#include "Viewport.h"

#include <numbers>
#include <cmath>

namespace outshine::Gltf {

bool Camera::Projection(double viewportAspect, Transform &out) const {
  for (double &element : out.M) { element = 0; }

  if (Kind == CameraKind::Perspective) {
    if (!(viewportAspect > 0) || !(YfovRad > 0) || YfovRad >= std::numbers::pi || !(ZNearM > 0)) {
      return false;
    }
    const double cotangent = 1.0 / std::tan(0.5 * YfovRad);
    out.M[0] = cotangent / viewportAspect;
    out.M[5] = cotangent;
    out.M[11] = -1;
    if (ZFarM > 0) {
      if (!(ZFarM > ZNearM)) { return false; }
      out.M[10] = (ZFarM + ZNearM) / (ZNearM - ZFarM);
      out.M[14] = (2 * ZFarM * ZNearM) / (ZNearM - ZFarM);
    } else {
      out.M[10] = -1;
      out.M[14] = -2 * ZNearM;
    }
    return true;
  }

  if (!(XMagM > 0) || !(YMagM > 0) || !(ZFarM > ZNearM)) { return false; }
  out.M[0] = 1.0 / XMagM;
  out.M[5] = 1.0 / YMagM;
  out.M[10] = 2.0 / (ZNearM - ZFarM);
  out.M[14] = (ZFarM + ZNearM) / (ZNearM - ZFarM);
  out.M[15] = 1;
  return true;
}

}
