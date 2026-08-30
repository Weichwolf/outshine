#include "Axes.h"

namespace outshine::Gltf {

void InEcef(const double gltf[3], double out[3]) {
  out[0] = gltf[1];
  out[1] = gltf[0];
  out[2] = -gltf[2];
}

void PlacedInEcef(const double gltf[16], double out[16]) {
  constexpr int kAxis[4] = {1, 0, 2, 3};
  constexpr double kSign[4] = {1.0, 1.0, -1.0, 1.0};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      out[column * 4 + row] = kSign[row] * gltf[kAxis[column] * 4 + kAxis[row]] * kSign[column];
    }
  }
}

} // namespace outshine::Gltf
