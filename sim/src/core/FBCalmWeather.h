/* The weather hook's primitive floor, FBConstantElevation's sibling: still air, no cloud, unlimited
 * visibility, everywhere. THE DEFAULT for every client that does not declare otherwise, and the reason a
 * mission without a `wx` line is bit-identical to one from before weather existed. */
#ifndef FBCALMWEATHER_H
#define FBCALMWEATHER_H
#include "FBWeatherProvider.h"

namespace FlightBox {

class FBCalmWeather : public FBWeatherProvider {
public:
  FBWindNed WindNedMs(double latDeg, double lonDeg, double altM) const override {
    (void)latDeg; (void)lonDeg; (void)altM;
    return FBWindNed{};
  }
  FBCloudLayers CloudLayers(double latDeg, double lonDeg) const override {
    (void)latDeg; (void)lonDeg;
    return FBCloudLayers{};
  }
  double VisibilityM(double latDeg, double lonDeg) const override {
    (void)latDeg; (void)lonDeg;
    return kFBVisibilityUnlimitedM;
  }
};

} // namespace FlightBox
#endif
