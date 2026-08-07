/* The weather hook's primitive floor, ConstantElevation's sibling: still air, no cloud, unlimited
 * visibility, everywhere. THE DEFAULT for every client that does not declare otherwise, and the reason a
 * mission without a `wx` line is bit-identical to one from before weather existed. */
#ifndef CALMWEATHER_H
#define CALMWEATHER_H
#include "WeatherProvider.h"

namespace outshine {

class CalmWeather : public WeatherProvider {
public:
  WindNed WindNedMs(double latDeg, double lonDeg, double altM) const override {
    (void)latDeg; (void)lonDeg; (void)altM;
    return WindNed{};
  }
  CloudLayers Clouds(double latDeg, double lonDeg) const override {
    (void)latDeg; (void)lonDeg;
    return CloudLayers{};
  }
  double VisibilityM(double latDeg, double lonDeg) const override {
    (void)latDeg; (void)lonDeg;
    return kFBVisibilityUnlimitedM;
  }
};

} // namespace outshine
#endif
