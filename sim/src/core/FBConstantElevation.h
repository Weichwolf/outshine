/* The elevation hook's primitive floor: one fixed ground height everywhere, so a ground mission runs
 * with no elevation data at all. Default 0 m = sea level, this codebase's "no data" fallback. */
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
