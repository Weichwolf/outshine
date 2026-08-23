#ifndef OUTSHINE_CORE_CONSTANTWINDWEATHER_H
#define OUTSHINE_CORE_CONSTANTWINDWEATHER_H
#include <cmath>
#include "CalmWeather.h"
#include "Units.h"

namespace outshine {

class ConstantWindWeather : public CalmWeather {
public:

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

}
#endif
