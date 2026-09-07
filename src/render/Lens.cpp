#include "Lens.h"
#include "math/Units.h"
#include <cassert>
#include <cmath>

namespace outshine::Render {

Mat4f Lens::Projection() const {
  const double widePx = WidePx;
  const double highPx = HighPx;
  const float asp = static_cast<float>(widePx) / static_cast<float>(highPx);
  const float zn = NearM;
  assert(OrthoM > 0.0f || FovDeg > 0.0f);
  const float f =
      OrthoM > 0.0f ? 0.0f : 1.0f / std::tan(FovDeg * static_cast<float>(kDeg2Rad) / 2.0f);
  Mat4f p = {{f / asp, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, zn, 0}};

  if (FarM > 0.0f && std::isfinite(FarM)) {
    p[10] = zn / (FarM - zn);
    p[14] = FarM * p[10];
  }

  const float ndcX = widePx > 0 ? 2.0f * Jitter[0] / static_cast<float>(widePx) : 0.0f;
  const float ndcY = highPx > 0 ? 2.0f * Jitter[1] / static_cast<float>(highPx) : 0.0f;
  p[8] = -ndcX;
  p[9] = -ndcY;
  if (OrthoM > 0.0f) {
    const float hw = 0.5f * OrthoWidthM;
    const float hh = 0.5f * OrthoM;
    const float zf = FarM;
    const float rz = 1.0f / (zf - zn);
    Mat4f q = {{1.0f / hw, 0, 0, 0, 0, 1.0f / hh, 0, 0, 0, 0, rz, 0, 0, 0, zf * rz, 1}};
    q[12] = ndcX;
    q[13] = ndcY;
    for (int i = 0; i < 16; i++) { p[i] = q[i]; }
  }
  return p;
}

} // namespace outshine::Render
