#ifndef OUTSHINE_RENDER_PRISM_CONVEXPRISM_H
#define OUTSHINE_RENDER_PRISM_CONVEXPRISM_H

#include "Box.h"
#include "Mat4.h"
#include <array>

namespace outshine::Render {
struct ConvexPrism {
  Mat4f SceneFromLocal;
  Boxf Bounds;
  // Local points satisfy dot(Plane.xyz, point) <= Plane.w for all seven planes.
  std::array<std::array<float, 4>, 7> Planes;
};
}
#endif
