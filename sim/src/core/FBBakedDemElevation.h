/* FlightBox — FBBakedDemElevation: the elevation hook's offline-DEM implementation ("--elev swiss") —
 * loads a small baked island raster (sim/assets, see tools/bake_swiss_dem.py) once and answers
 * GroundElevM by bilinear interpolation, 0 m outside the raster's bbox (the "island" contract: the
 * asset only covers Switzerland, everything else reads sea level). core/, not world/: it is a static
 * data asset load (fopen once, no network, no streaming), the same category of file I/O JSBSim itself
 * does for its own model XML — not the "app owns file I/O" telemetry-sink exception. */
#ifndef FBBAKEDDEMELEVATION_H
#define FBBAKEDDEMELEVATION_H
#include <cstdint>
#include <string>
#include <vector>
#include "FBElevationProvider.h"

namespace FlightBox {

class FBBakedDemElevation : public FBElevationProvider {
public:
  /* Loads `path` (the .bin format tools/bake_swiss_dem.py writes — see FBBakedDemElevation.cpp's
   * banner for the layout). Ok() is false on any read/format failure; GroundElevM then always
   * returns 0 (degrades to the flat-sea-level fallback rather than crashing a mission boot). */
  explicit FBBakedDemElevation(const std::string &path);

  bool Ok(void) const { return LoadedOk; }
  double GroundElevM(double latDeg, double lonDeg) const override;

private:
  bool LoadedOk = false;
  double LonMin = 0.0, LonMax = 0.0, LatMin = 0.0, LatMax = 0.0;
  uint32_t Cols = 0, Rows = 0;
  float ScaleM = 1.0f;
  std::vector<int16_t> Grid;   /* row-major, row 0 = LatMin (south), col 0 = LonMin (west) */
};

} // namespace FlightBox
#endif
