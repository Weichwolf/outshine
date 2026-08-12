/* THE SLIPPY-TILE LATTICE AND ITS REGISTRATION. Every DEM sample in this tree goes through the two
 * lines at the bottom of this file, so a texel means one thing here.
 *
 * WHERE THE SCHEME ENDS is stated ONCE. What clamps (GeoToTileClamped) and what refuses by name
 * (TileIndex, TileGeodesy.h) have to do it at the same latitude, and until this round they did not:
 * the band was spelled 85.0511287798 on one side of a process boundary and 85.05112877980659 on the
 * other, which is the eleventh digit and a defect either way. */
#ifndef TILEMATH_H
#define TILEMATH_H

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace outshine::World {

/* atan(sinh(pi)) in degrees — the WMTS / OGC simple-tile-scheme bound. Beyond it there is no tile at
 * any zoom, and no caller may invent one. */
constexpr double kMercatorLatMaxDeg = 85.05112877980659;

constexpr double kPi = 3.14159265358979323846;

inline double ClampD(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* A RASTER TEXEL IS AN AREA, NOT A LATTICE POINT: tilezen/joerd writes a Terrarium tile through a
 * GDAL geotransform of the tile's own bbox with res = span/N (joerd/mercator.py), so texel i covers
 * [i/N,(i+1)/N] and its sample sits at (i+0.5)/N. A query at tile fraction f therefore lands at grid
 * index f*N - 0.5 — half a texel west/north of the naive f*N, and one full texel away from f*(N-1)
 * at the far edge. */
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

}  // namespace outshine::World
#endif
