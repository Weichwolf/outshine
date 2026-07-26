/* FlightBox — FBRunwayPlateauElevation: the gym's DEM-free elevation provider ("--elev const"). A
 * mission can have more than one runway (today one; Phase 3's dest_runway will add a second) at
 * DIFFERENT elevations, so a single flat constant is wrong for takeoff+landing in the same run. This
 * provider instead holds every runway the mission cares about and answers each query with: inside that
 * runway's footprint (its length x width rectangle) + a ~5 km margin, the runway's own
 * ThresholdElevM; beyond that, a ~10 km smoothstep falloff onto a flat 0 m base, so enroute cruise
 * still reads a plausible (if approximate) AGL instead of a hard cliff at the footprint edge.
 * Overlapping plateaus follow the NEAREST runway only (simplest continuous choice — a hard switch
 * between two plateaus of different elevation is possible only where their footprints are close
 * enough to overlap, which real airfields never are; document, don't over-engineer for a case that
 * cannot occur with today's one-runway missions). */
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
