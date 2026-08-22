#ifndef TILEMATH_H
#define TILEMATH_H

#include <cmath>
#include <numbers>
#include <cstddef>
#include <cstdint>

#include "Mercator.h"
#include "Units.h"

namespace outshine::World {


inline double ClampD(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline double TexelIndex(double frac, uint32_t n) { return frac * (double)n - 0.5; }

inline float Bilinear(const float *g, uint32_t cols, uint32_t rows, double gx, double gy) {
  if (!g || !cols || !rows) return 0.f;
  gx = ClampD(gx, 0.0, (double)cols - 1.0);
  gy = ClampD(gy, 0.0, (double)rows - 1.0);
  const uint32_t x0 = (uint32_t)gx, y0 = (uint32_t)gy;
  const uint32_t x1 = x0 + 1 < cols ? x0 + 1 : x0;
  const uint32_t y1 = y0 + 1 < rows ? y0 + 1 : y0;
  const double fx = gx - x0, fy = gy - y0;
  const double a = g[(size_t)y0 * cols + x0], b = g[(size_t)y0 * cols + x1];
  const double c = g[(size_t)y1 * cols + x0], d = g[(size_t)y1 * cols + x1];
  return (float)((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy);
}

}
#endif
