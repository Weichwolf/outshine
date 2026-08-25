#ifndef OUTSHINE_CONTENT_GLTF_VIEWPORT_H
#define OUTSHINE_CONTENT_GLTF_VIEWPORT_H

#include <string>

#include "Transform.h"
#include "Types.h"

namespace outshine::Gltf {

struct Viewport {
  double WidthPx = 0;
  double HeightPx = 0;

  double Aspect() const { return (HeightPx > 0) ? WidthPx / HeightPx : 0.0; }
  void Raster(const double ndc[3], double outPx[2]) const {
    outPx[0] = (ndc[0] * 0.5 + 0.5) * WidthPx - 0.5;
    outPx[1] = (0.5 - ndc[1] * 0.5) * HeightPx - 0.5;
  }
};

struct Camera {
  std::string Name;
  CameraKind Kind = CameraKind::Perspective;

  double YfovRad = 0;

  double AspectRatio = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  double XMagM = 0;
  double YMagM = 0;

  [[nodiscard]] bool Projection(double viewportAspect, Transform &out) const;
};

}
#endif
