/* A MEASURING INSTRUMENT, and deliberately unphysical: one wind vector at every point of the earth and
 * every altitude, no shear, no boundary layer. It exists so a mission can hold the wind constant and
 * make drift the only variable — the way a wind tunnel is not a sky. Real weather is FixedWeather. */
#ifndef CONSTANTWINDWEATHER_H
#define CONSTANTWINDWEATHER_H
#include <cmath>
#include "CalmWeather.h"
#include "Units.h"

namespace outshine {

class ConstantWindWeather : public CalmWeather {
public:
  /* Declared the way a wind is REPORTED — the bearing it comes FROM, in knots — and converted here
   * once, so no caller has to remember which convention the vector below is in. */
  ConstantWindWeather(double fromDeg, double speedKt) {
    double v = speedKt * kKtToMs, a = fromDeg * kDeg2Rad;
    Wind_.N = -v * std::cos(a);
    Wind_.E = -v * std::sin(a);
  }

  WindNed WindNedMs(double latDeg, double lonDeg, double altM) const override {
    (void)latDeg; (void)lonDeg; (void)altM;
    return Wind_;
  }

private:
  WindNed Wind_;
};

} // namespace outshine
#endif
