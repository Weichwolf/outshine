#include "SurfaceMaterial.h"
#include <cmath>
#include <cstddef>

namespace outshine::Ground {

Vec4f SurfaceMaterial::MeanGround() const noexcept {
  Vec4f mean;
  for (size_t channel = 0; channel < 4; ++channel) {
    mean[channel] = std::lerp(Ground[channel], Litter[channel], Mix[0]);
  }
  return mean;
}

}
