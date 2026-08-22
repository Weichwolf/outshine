#ifndef PIXELFOCALLENGTH_H
#define PIXELFOCALLENGTH_H

#include <numbers>
#include <cmath>

namespace outshine {

inline double PixelFocalLength(int heightPx, double fovDeg) {
  return 0.5 * (double)heightPx / std::tan(0.5 * fovDeg * (std::numbers::pi / 180.0));
}

}
#endif
