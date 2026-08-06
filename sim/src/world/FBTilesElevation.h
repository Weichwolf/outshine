/* The elevation hook's live-DEM implementation: a thin pass-through onto fb_stream_ground. In world/
 * and not core/, because it depends on the tile-streaming C ABI the core library excludes. */
#ifndef FBTILESELEVATION_H
#define FBTILESELEVATION_H
#include <cstdint>
#include <vector>
#include "FBElevationProvider.h"
#include "FBTerrainLoader.h"

namespace FlightBox::World {

class FBTilesElevation : public FBElevationProvider {
public:
  /* fb_stream_ground reads the base URL fb_stream_open set, independent of that call's (lat,lon) —
   * which only seeds the render-side quadtree root. So `base` is all this needs. */
  explicit FBTilesElevation(const char *base) { fb_stream_open(base, 0.0, 0.0, 8); }

  double GroundElevM(double latDeg, double lonDeg) const override {
    return fb_stream_ground(latDeg, lonDeg);
  }

  /* THE BATCH DECODE the base class left open. Both sample whole Terrarium DEM TILES — the same ones
   * the mesh is built from, so a patch and the terrain a frame draws cannot disagree — but this one
   * picks its ZOOM from the patch's own post spacing, while GroundElevM is fixed at the zoom /elev
   * uses. A 20 nm radar-map patch therefore touches a 4x4 field instead of the ~100 z13 tiles the
   * default loop would fetch one point at a time. */
  bool GroundElevPatch(double latMinDeg, double lonMinDeg, double latMaxDeg, double lonMaxDeg,
                       int cols, int rows, double *out) const override;

private:
  struct Tile {
    int Z = 0;
    int X = 0, Y = 0;
    uint32_t W = 0, H = 0;
    std::vector<float> Height;   /* row 0 = NORTH edge, as osmmesh_terrain_decode_png delivers it */
    bool Hole = false;           /* the server has no DEM here; do not keep asking within one patch */
  };

  /* Zoom whose texel is at most half the patch's own post spacing, so the patch never resamples a DEM
   * coarser than itself; clamped to what the tile set actually carries. */
  static int ZoomFor(double latDeg, double postSpacingM);
  const Tile *Fetch(int z, int x, int y) const;

  /* Small and LRU-free on purpose: one patch touches a handful of tiles and the next patch, one
   * rebuild later, touches nearly the same ones. Mutable because a provider is asked const. */
  mutable std::vector<Tile> Cache_;
};

} // namespace FlightBox::World
#endif
