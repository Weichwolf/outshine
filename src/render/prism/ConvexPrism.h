#ifndef OUTSHINE_RENDER_PRISM_CONVEXPRISM_H
#define OUTSHINE_RENDER_PRISM_CONVEXPRISM_H

#include "Box.h"
#include "Mat4.h"
#include <array>
#include <cstdint>

namespace outshine::Render {
struct ConvexPrism {
  Mat4f SceneFromLocal;
  Boxf Bounds;
  // Local points satisfy dot(Plane.xyz, point) <= Plane.w for all seven planes.
  std::array<std::array<float, 4>, 7> Planes;
  uint32_t SecondaryFaces = 0;
  // Vertex tint and a roughness override. Negative roughness uses the registered material.
  std::array<float, 4> Primary{{1, 1, 1, -1}};
  std::array<float, 4> Secondary{{1, 1, 1, -1}};
};
}
#endif
