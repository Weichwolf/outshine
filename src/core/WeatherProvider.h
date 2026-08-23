#ifndef OUTSHINE_CORE_WEATHERPROVIDER_H
#define OUTSHINE_CORE_WEATHERPROVIDER_H

namespace outshine {

struct WindNed {
  double N = 0.0, E = 0.0, D = 0.0;
};

struct CloudLayers {
  double TotalPct = 0.0, LowPct = 0.0, MidPct = 0.0, HighPct = 0.0;
  double CeilingM = 0.0;
  bool   HaveCeiling = false;
};

constexpr double kFBVisibilityUnlimitedM = 100000.0;

class WeatherProvider {
public:
  virtual ~WeatherProvider() = default;

  virtual WindNed WindNedMs(double latDeg, double lonDeg, double altM) const = 0;

  virtual CloudLayers Clouds(double latDeg, double lonDeg) const = 0;

  virtual double VisibilityM(double latDeg, double lonDeg) const = 0;
};

}
#endif
