#include "Viewport.h"
#include "math/Projection.h"

namespace outshine::Gltf {
bool Camera::Projection(double viewportAspect, Mat4 &out) const {
  if (Kind == CameraKind::Perspective) {
    return ProjectionMatrix(
        PerspectiveProjection{.VerticalFovRad = YfovRad, .NearM = ZNearM, .FarM = ZFarM},
        viewportAspect,
        out);
  }
  return ProjectionMatrix(
      OrthographicProjection{
          .HalfWidthM = XMagM, .HalfHeightM = YMagM, .NearM = ZNearM, .FarM = ZFarM},
      out);
}
}
