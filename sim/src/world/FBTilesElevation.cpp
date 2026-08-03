#include "FBTilesElevation.h"

#include <cmath>

#include "FBElevationProvider.h"
#include "terrain.h"

namespace FlightBox::World {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthCircM = 40075016.686;
/* What fb-tiles actually bakes. Below 8 a patch would resample a continent; above 13 the DEM source
 * has no more detail and every extra level is four times the fetches for the same heights. */
constexpr int kMinZ = 8, kMaxZ = 13;
constexpr size_t kMaxCachedTiles = 24;   /* a 20 nm patch touches at most 4x4 at the chosen zoom */

double TileXf(double lonDeg, int z) { return (lonDeg + 180.0) / 360.0 * (double)(1u << z); }
double TileYf(double latDeg, int z) {
  double r = latDeg * kPi / 180.0;
  return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / kPi) / 2.0 * (double)(1u << z);
}
} // namespace

int FBTilesElevation::ZoomFor(double latDeg, double postSpacingM) {
  if (!(postSpacingM > 0.0)) return kMaxZ;
  double c = std::cos(latDeg * kPi / 180.0);
  if (c < 1e-3) c = 1e-3;
  /* texel(z) = circumference * cos(lat) / (2^z * 256); want texel <= postSpacing/2. */
  double want = kEarthCircM * c / (256.0 * (postSpacingM * 0.5));
  int z = (int)std::ceil(std::log2(want > 1.0 ? want : 1.0));
  return z < kMinZ ? kMinZ : (z > kMaxZ ? kMaxZ : z);
}

const FBTilesElevation::Tile *FBTilesElevation::Fetch(int z, int x, int y) const {
  for (const Tile &t : Cache_)
    if (t.Z == z && t.X == x && t.Y == y) return t.Hole ? nullptr : &t;
  if (Cache_.size() >= kMaxCachedTiles) Cache_.clear();   /* a patch is coherent; a whole flush is the cheapest eviction */
  Cache_.push_back(Tile{z, x, y, 0, 0, {}, true});
  Tile &slot = Cache_.back();
  const uint8_t *bytes = nullptr;
  int len = 0;
  if (fb_stream_dem(z, x, y, &bytes, &len) != 1 || !bytes || len <= 0) return nullptr;
  osmmesh_terrain_grid grid{};
  if (osmmesh_terrain_decode_png(bytes, (size_t)len, &grid) != OSMMESH_TERRAIN_OK) return nullptr;
  slot.W = grid.cols;
  slot.H = grid.rows;
  slot.Height.assign(grid.heights, grid.heights + (size_t)grid.cols * grid.rows);
  osmmesh_terrain_grid_free(&grid);
  slot.Hole = false;
  return &slot;
}

bool FBTilesElevation::GroundElevPatch(double latMinDeg, double lonMinDeg, double latMaxDeg,
                                       double lonMaxDeg, int cols, int rows, double *out) const {
  if (!out || cols < 2 || rows < 2) return false;
  double latMid = 0.5 * (latMinDeg + latMaxDeg);
  double spanM = (latMaxDeg - latMinDeg) * kEarthCircM / 360.0;
  int z = ZoomFor(latMid, spanM / (double)(rows - 1));
  double n = (double)(1u << z);

  for (int r = 0; r < rows; r++) {
    double lat = latMinDeg + (latMaxDeg - latMinDeg) * (double)r / (double)(rows - 1);
    double ty = TileYf(lat, z);
    int tyi = (int)std::floor(ty);
    for (int c = 0; c < cols; c++) {
      double lon = lonMinDeg + (lonMaxDeg - lonMinDeg) * (double)c / (double)(cols - 1);
      double tx = TileXf(lon, z);
      int txi = (int)std::floor(tx);
      const Tile *t = (txi >= 0 && tyi >= 0 && txi < (int)n && tyi < (int)n) ? Fetch(z, txi, tyi) : nullptr;
      double h = kFBElevationUnresolved;
      if (t && t->W > 1 && t->H > 1) {
        /* Bilinear INSIDE the tile; a sample in the outermost half-texel clamps rather than reaching
         * into the neighbour — under one texel of error at a seam, well inside a range bin. */
        double px = (tx - (double)txi) * (double)t->W - 0.5;
        double py = (ty - (double)tyi) * (double)t->H - 0.5;
        if (px < 0.0) px = 0.0;
        if (py < 0.0) py = 0.0;
        if (px > (double)(t->W - 1)) px = (double)(t->W - 1);
        if (py > (double)(t->H - 1)) py = (double)(t->H - 1);
        uint32_t x0 = (uint32_t)px, y0 = (uint32_t)py;
        uint32_t x1 = x0 + 1 < t->W ? x0 + 1 : x0, y1 = y0 + 1 < t->H ? y0 + 1 : y0;
        double fx = px - (double)x0, fy = py - (double)y0;
        const float *g = t->Height.data();
        double a = g[(size_t)y0 * t->W + x0], b = g[(size_t)y0 * t->W + x1];
        double cc = g[(size_t)y1 * t->W + x0], d = g[(size_t)y1 * t->W + x1];
        h = (a * (1 - fx) + b * fx) * (1 - fy) + (cc * (1 - fx) + d * fx) * fy;
        if (h < 0.0) h = 0.0;   /* the same sea-level clamp fb_stream_ground applies to its samples */
      }
      out[(size_t)r * cols + c] = h;
    }
  }
  return true;
}

} // namespace FlightBox::World
