/* The elevation hook's live-DEM implementation: a thin pass-through onto fb_stream_ground. In world/
 * and not core/, because it depends on the tile-streaming C ABI the core library excludes. */
#ifndef FBTILESELEVATION_H
#define FBTILESELEVATION_H
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
};

} // namespace FlightBox::World
#endif
