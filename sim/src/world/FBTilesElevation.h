/* FlightBox — FBTilesElevation: the elevation hook's live-DEM implementation for the native and WASM
 * clients — a thin pass-through onto the existing fb_stream_ground/fb_stream_open terrain-streaming
 * mechanism (FBTerrainLoader.h), so ground-truth consumers go through FBElevationProvider without any
 * change in the numbers fb_stream_ground already produced. world/, not core/: it depends on the tile-
 * streaming C ABI that render/world own, which the core library intentionally excludes. */
#ifndef FBTILESELEVATION_H
#define FBTILESELEVATION_H
#include "FBElevationProvider.h"
#include "FBTerrainLoader.h"

namespace FlightBox {

class FBTilesElevation : public FBElevationProvider {
public:
  /* fb_stream_ground reads the fb_base fb_stream_open sets, independent of the (lat,lon) passed to
   * fb_stream_open itself (that origin only seeds the render-side quadtree root, irrelevant to a plain
   * /elev point query) — so this constructor only needs `base`, no mission/runway knowledge yet. */
  explicit FBTilesElevation(const char *base) { fb_stream_open(base, 0.0, 0.0, 8); }

  double GroundElevM(double latDeg, double lonDeg) const override {
    return fb_stream_ground(latDeg, lonDeg);
  }
};

} // namespace FlightBox
#endif
