#include <array>
#include "math/Mat4.h"
#include "math/Vec4.h"
#include "Axes.h"
#include "math/Vec3.h"

namespace outshine::Gltf {

void InEcef(const Vec3 &gltf, Vec3 &out) {
  out[0] = gltf[1];
  out[1] = gltf[0];
  out[2] = -gltf[2];
}

void PlacedInEcef(const Mat4 &gltf, Mat4 &out) {
  constexpr std::array<int, 4> kAxis = {{1, 0, 2, 3}};
  constexpr Vec4 kSign = {{1.0, 1.0, -1.0, 1.0}};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      out[column * 4 + row] = kSign[row] * gltf[kAxis[column] * 4 + kAxis[row]] * kSign[column];
    }
  }
}

} // namespace outshine::Gltf
