#ifndef OUTSHINE_RENDER_LENS_H
#define OUTSHINE_RENDER_LENS_H

#include "math/Mat4.h"
#include "math/Vec2.h"

namespace outshine::Render {

struct Lens {
  double WidePx = 0;
  double HighPx = 0;
  float FovDeg = 0.0f;
  float OrthoWidthM = 0.0f;
  float OrthoM = 0.0f;
  float NearM = 0.0f;
  float FarM = 0.0f;
  Vec2f Jitter = {{0.0f, 0.0f}};

  [[nodiscard]] Mat4f Projection() const;
};

} // namespace outshine::Render

#endif
