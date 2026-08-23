#ifndef OUTSHINE_SCENARIO_STANDPOINT_H
#define OUTSHINE_SCENARIO_STANDPOINT_H

#include <optional>

#include "Mercator.h"

namespace outshine::SceneLegacy {

class Standpoint {
public:
  static std::optional<Standpoint> At(double latDeg, double lonDeg) noexcept {
    if (latDeg < -kMercatorLatMaxDeg || latDeg > kMercatorLatMaxDeg) return std::nullopt;
    if (lonDeg < -180.0 || lonDeg > 180.0) return std::nullopt;
    return Standpoint(latDeg, lonDeg);
  }

  double LatDeg() const noexcept { return Lat_; }
  double LonDeg() const noexcept { return Lon_; }

private:
  Standpoint(double latDeg, double lonDeg) noexcept : Lat_(latDeg), Lon_(lonDeg) {}

  double Lat_, Lon_;
};

}
#endif
