#ifndef PIXELFOCALLENGTH_H
#define PIXELFOCALLENGTH_H

#include <cmath>

namespace outshine {

inline double PixelFocalLength(int heightPx, double fovDeg) {
  return 0.5 * (double)heightPx / std::tan(0.5 * fovDeg * 0.017453292519943295);
}

}
#endif
