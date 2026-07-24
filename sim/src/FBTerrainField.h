/* FlightBox — FBTerrainField: a coarse-DEM terrain-height oracle for the LOWLEVEL autopilot. Samples
 * Terrarium DEM tiles at a fixed zoom (default 11 ~ 50 m/px) via fb_stream_dem, decodes them to float
 * height grids (osmmesh), keeps a small count-capped LRU of decoded grids, and answers HeightAt(lat,lon)
 * by bilinear interpolation. This is the shared terrain source for the vertical AGL-hold look-ahead and
 * (Stage 2) the lateral path planner — NOT the render mesh path (that stays in FBWorld/osmmesh). Cheap
 * after the first decode: a moving corridor re-hits the same 1-2 cached grids. Decode failures / DEM
 * holes read as sea level (0 m). */
#ifndef FBTERRAINFIELD_H
#define FBTERRAINFIELD_H

#include <cstdint>
#include "terrain.h"

namespace FlightBox {

class FBTerrainField {
public:
  explicit FBTerrainField(int zoom = 11);
  ~FBTerrainField();
  FBTerrainField(const FBTerrainField &) = delete;
  FBTerrainField &operator=(const FBTerrainField &) = delete;

  int  Zoom(void) const { return Z; }

  /* Terrain height (m ASL) at (lat,lon), bilinear over the coarse DEM. A miss/hole/decode-fail reads as
   * sea level 0. `valid` (optional) reports whether a real DEM tile backed the sample (false = fell back
   * to 0). Synchronous: on the native oracle fb_stream_dem blocks briefly on a cold tile, then caches. */
  float HeightAt(double latDeg, double lonDeg, bool *valid = nullptr);

  long  Decodes(void) const { return DecodeCount; }   /* telemetry: cold tile decodes (LRU misses) */

private:
  static constexpr int kCap = 64;   /* resident decoded grids; a 15 km corridor needs 1-2, a plan path more */
  struct Ent { int z, x, y; uint64_t used; osmmesh_terrain_grid grid; };

  Ent *Acquire(int x, int y);   /* LRU get-or-fetch-or-decode the tile (z fixed); nullptr on a hole */

  int Z;
  Ent Cache[kCap];
  int N;              /* live entries */
  uint64_t Clock;     /* LRU stamp */
  long DecodeCount;
};

} // namespace FlightBox
#endif
