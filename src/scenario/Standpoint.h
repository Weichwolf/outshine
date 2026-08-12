/* A PLACE ON EARTH THIS ENGINE CAN BE ENTERED FROM. There is no constructor: the only way to hold one
 * is to have got it from At(), which refuses a latitude the slippy-tile scheme cannot carry
 * (core/Mercator.h). So "a standpoint outside the Mercator band" is not a value that is checked
 * somewhere — it is a value that does not exist, and a stage with no place has no field to put one in. */
#ifndef STANDPOINT_H
#define STANDPOINT_H

#include <optional>

#include "Mercator.h"

namespace outshine::Scenario {

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

} // namespace outshine::Scenario
#endif
