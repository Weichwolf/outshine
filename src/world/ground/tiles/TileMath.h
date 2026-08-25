#ifndef OUTSHINE_WORLD_GROUND_TILES_TILEMATH_H
#define OUTSHINE_WORLD_GROUND_TILES_TILEMATH_H

#include <cmath>
#include <numbers>
#include <cstddef>
#include <cstdint>

#include "Mercator.h"
#include <mdspan>

#include "Units.h"

namespace outshine::Ground {


inline double ClampD(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

using Postings = std::mdspan<const float, std::dextents<size_t, 2>>;
static_assert(Postings::rank() == 2,
              "a field is two extents that travel together, never a pointer beside a stride");

[[nodiscard]] inline float Bilinear(Postings field, double gx, double gy) {
  const size_t rows = field.extent(0), cols = field.extent(1);
  if (field.data_handle() == nullptr || cols == 0 || rows == 0) { return 0.f; }
  gx = ClampD(gx, 0.0, (double)cols - 1.0);
  gy = ClampD(gy, 0.0, (double)rows - 1.0);
  const size_t x0 = (size_t)gx, y0 = (size_t)gy;
  const size_t x1 = x0 + 1 < cols ? x0 + 1 : x0;
  const size_t y1 = y0 + 1 < rows ? y0 + 1 : y0;
  const double fx = gx - (double)x0, fy = gy - (double)y0;
  const double a = field[y0, x0], b = field[y0, x1];
  const double c = field[y1, x0], d = field[y1, x1];
  return (float)((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy);
}

}
#endif
