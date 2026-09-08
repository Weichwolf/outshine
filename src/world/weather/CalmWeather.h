#ifndef OUTSHINE_WORLD_WEATHER_CALMWEATHER_H
#define OUTSHINE_WORLD_WEATHER_CALMWEATHER_H
#include "WeatherProvider.h"

namespace outshine {

class CalmWeather : public WeatherProvider {
public:
  WindNed WindNedMs(double latDeg, double lonDeg, double altM) const override {
    (void)latDeg;
    (void)lonDeg;
    (void)altM;
    return WindNed{};
  }

  CloudLayers Clouds(double latDeg, double lonDeg) const override {
    (void)latDeg;
    (void)lonDeg;
    return CloudLayers{};
  }

  double VisibilityM(double latDeg, double lonDeg) const override {
    (void)latDeg;
    (void)lonDeg;
    return kFBVisibilityUnlimitedM;
  }
};

}
#endif
