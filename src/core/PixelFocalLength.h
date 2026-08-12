#ifndef PIXELFOCALLENGTH_H
#define PIXELFOCALLENGTH_H

#include <cmath>

namespace outshine {

/* Pixels. A span of `sizeM` metres at `distM` metres covers `sizeM * PixelFocalLength / distM`
 * pixels — which is the whole screen-space error metric every LOD ladder in the engine measures
 * against, so it is defined once and never spelled out at a call site. */
inline double PixelFocalLength(int heightPx, double fovDeg) {
  return 0.5 * (double)heightPx / std::tan(0.5 * fovDeg * 0.017453292519943295);
}

} // namespace outshine
#endif
