#include "FBTerrainField.h"
#include "FBTerrainLoader.h"
#include <cmath>
#include <cstring>

namespace FlightBox {

static const double kPi = 3.14159265358979323846;

FBTerrainField::FBTerrainField(int zoom) : Z(zoom > 0 ? zoom : 11), N(0), Clock(0), DecodeCount(0) {
  std::memset(Cache, 0, sizeof Cache);
}

FBTerrainField::~FBTerrainField() {
  for (int i = 0; i < N; i++) osmmesh_terrain_grid_free(&Cache[i].grid);
}

FBTerrainField::Ent *FBTerrainField::Acquire(int x, int y) {
  for (int i = 0; i < N; i++)
    if (Cache[i].z == Z && Cache[i].x == x && Cache[i].y == y) {
      Cache[i].used = ++Clock;
      return Cache[i].grid.heights ? &Cache[i] : nullptr;   /* cached hole -> nullptr, no refetch */
    }
  const uint8_t *bytes = nullptr;
  int len = 0;
  int r = fb_stream_dem(Z, x, y, &bytes, &len);
  if (r == 0) return nullptr;   /* PENDING (WASM async) -> no data yet, don't cache; retry next call */
  osmmesh_terrain_grid grid;
  std::memset(&grid, 0, sizeof grid);
  bool got = (r == 1) && bytes && len > 0 &&
             osmmesh_terrain_decode_png(bytes, (size_t)len, &grid) == OSMMESH_TERRAIN_OK &&
             grid.heights && grid.rows > 1 && grid.cols > 1;
  if (got) DecodeCount++;   /* r == -1 or a decode failure -> cache a permanent hole below */

  Ent *slot;
  if (N < kCap) {
    slot = &Cache[N++];
  } else {   /* evict LRU */
    slot = &Cache[0];
    for (int i = 1; i < kCap; i++)
      if (Cache[i].used < slot->used) slot = &Cache[i];
    osmmesh_terrain_grid_free(&slot->grid);
  }
  slot->z = Z; slot->x = x; slot->y = y; slot->used = ++Clock;
  if (got) slot->grid = grid;
  else std::memset(&slot->grid, 0, sizeof slot->grid);   /* cache the hole (heights == nullptr) */
  return got ? slot : nullptr;
}

float FBTerrainField::HeightAt(double latDeg, double lonDeg, bool *valid) {
  if (valid) *valid = false;
  /* Web-Mercator slippy fraction (matches osmmesh_geo_to_tile, full-precision fractional part). */
  while (lonDeg > 180.0) lonDeg -= 360.0;
  while (lonDeg < -180.0) lonDeg += 360.0;
  if (latDeg > 85.05) latDeg = 85.05;
  if (latDeg < -85.05) latDeg = -85.05;
  double n = (double)(1u << Z);
  double xf = (lonDeg + 180.0) / 360.0 * n;
  double latRad = latDeg * kPi / 180.0;
  double yf = (1.0 - std::asinh(std::tan(latRad)) / kPi) / 2.0 * n;
  int tx = (int)std::floor(xf), ty = (int)std::floor(yf);
  double fx = xf - tx, fy = yf - ty;

  Ent *e = Acquire(tx, ty);
  if (!e) return 0.0f;   /* hole / miss -> sea level */
  const osmmesh_terrain_grid &g = e->grid;
  double cf = fx * (double)(g.cols - 1), rf = fy * (double)(g.rows - 1);   /* col 0 = west, row 0 = north */
  int c0 = (int)std::floor(cf), r0 = (int)std::floor(rf);
  if (c0 < 0) c0 = 0;
  if (c0 > (int)g.cols - 2) c0 = (int)g.cols - 2;
  if (r0 < 0) r0 = 0;
  if (r0 > (int)g.rows - 2) r0 = (int)g.rows - 2;
  double tcx = cf - c0, tcy = rf - r0;
  const float *h = g.heights;
  double h00 = h[r0 * g.cols + c0], h01 = h[r0 * g.cols + c0 + 1];
  double h10 = h[(r0 + 1) * g.cols + c0], h11 = h[(r0 + 1) * g.cols + c0 + 1];
  double top = h00 + (h01 - h00) * tcx, bot = h10 + (h11 - h10) * tcx;
  if (valid) *valid = true;
  return (float)(top + (bot - top) * tcy);
}

} // namespace FlightBox
