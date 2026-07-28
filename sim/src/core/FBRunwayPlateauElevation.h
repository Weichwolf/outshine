/* The gym's DEM-free elevation provider (`--elev const`). A mission can have several runways at
 * DIFFERENT elevations, so a single flat constant is wrong for takeoff+landing in one run: this holds
 * every runway and answers with its own elevation inside footprint+margin, then a smoothstep falloff
 * onto a flat 0 m base. Overlapping plateaus follow the NEAREST runway — the simplest continuous
 * choice. doc/core.md, Abschnitt 9. */
#ifndef FBRUNWAYPLATEAUELEVATION_H
#define FBRUNWAYPLATEAUELEVATION_H
#include <vector>
#include "FBElevationProvider.h"
#include "FBRunway.h"

namespace FlightBox {

class FBRunwayPlateauElevation : public FBElevationProvider {
public:
  explicit FBRunwayPlateauElevation(std::vector<FBRunway> runways, double baseElevM = 0.0)
      : Runways(std::move(runways)), BaseElevM(baseElevM) {}

  double GroundElevM(double latDeg, double lonDeg) const override;

private:
  std::vector<FBRunway> Runways;
  double BaseElevM;
};

} // namespace FlightBox
#endif
