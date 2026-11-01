#ifndef OUTSHINE_ENGINE_GROUNDVIEW_H
#define OUTSHINE_ENGINE_GROUNDVIEW_H

#include "Viewing.h"
#include <cmath>
#include <numbers>

namespace outshine {
inline constexpr auto kInvalidGroundView =
    "ground detail needs a finite camera eye, positive lens and frame height";

[[nodiscard]] inline bool ValidGroundView(const Render::Viewpoint &view, int heightPx) noexcept {
  if (heightPx <= 0) { return false; }
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(view.EyeM[axis])) { return false; }
  }
  if (view.Kind == Render::CameraKind::Orthographic) {
    return std::isfinite(view.YMagM) && view.YMagM > 0.0;
  }
  return std::isfinite(view.YfovRad) && view.YfovRad > 0.0 && view.YfovRad < std::numbers::pi;
}

}
#endif
