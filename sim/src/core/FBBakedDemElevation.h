/* The elevation hook's offline-DEM implementation (`--elev swiss`): one baked island raster, bilinear,
 * 0 m outside its bbox. core/ and not world/ because it is a static asset load, the same category of
 * file I/O JSBSim does for its own model XML. doc/flightbox/core.md, Abschnitt 9. */
#ifndef FBBAKEDDEMELEVATION_H
#define FBBAKEDDEMELEVATION_H
#include <cstdint>
#include <string>
#include <vector>
#include "FBElevationProvider.h"

namespace FlightBox {

class FBBakedDemElevation : public FBElevationProvider {
public:
  /* Ok() false on any read/format failure; GroundElevM then returns 0 — degrading to sea level rather
   * than crashing a mission boot. */
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
