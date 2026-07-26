/* FlightBox — FBConstantElevation: the elevation-hook default — one fixed ground height everywhere,
 * settable at construction. The gym client sets it to the mission's own runway threshold elevation
 * (FBRunway::ThresholdElevM) automatically, so a ground mission runs with zero elevation data at all
 * (Prinzip 4-friendly: deterministic, no network). Default 0 m matches every other "no data yet"
 * fallback in this codebase (sea level). */
#ifndef FBCONSTANTELEVATION_H
#define FBCONSTANTELEVATION_H
#include "FBElevationProvider.h"

namespace FlightBox {

class FBConstantElevation : public FBElevationProvider {
public:
  explicit FBConstantElevation(double elevM = 0.0) : ElevM(elevM) {}
  double GroundElevM(double, double) const override { return ElevM; }
  void SetElevM(double elevM) { ElevM = elevM; }

private:
  double ElevM;
};

} // namespace FlightBox
#endif
