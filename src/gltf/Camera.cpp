#include "Camera.h"

#include <cmath>

namespace outshine::Gltf {

bool Camera::Projection(double viewportAspect, Transform &out) const {
  for (double &element : out.M) { element = 0; }

  if (Kind == CameraKind::Perspective) {
    /* THE VIEWPORT DECIDES THE HORIZONTAL EXTENT, never the file's `aspectRatio`. Dividing by the
     * file's number in a viewport of another shape scales x and y differently, which turns a circle
     * into an ellipse; `Cameras` states 1.0 against a 16:9 frame and is the first asset here that
     * reaches the question. Vertical field of view is kept and the horizontal one follows the
     * frame, which is what Blender's importer and Khronos's own viewer both do. */
    if (!(viewportAspect > 0) || !(YfovRad > 0) || YfovRad >= 3.14159265358979323846 ||
        !(ZNearM > 0)) {
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

} // namespace outshine::Gltf
