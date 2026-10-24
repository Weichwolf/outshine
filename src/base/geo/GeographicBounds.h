#ifndef OUTSHINE_BASE_GEO_GEOGRAPHICBOUNDS_H
#define OUTSHINE_BASE_GEO_GEOGRAPHICBOUNDS_H

namespace outshine {
struct GeographicBounds {
  double WestDeg = 0.0;
  double SouthDeg = 0.0;
  double EastDeg = 0.0;
  double NorthDeg = 0.0;

  [[nodiscard]] bool operator==(const GeographicBounds &) const = default;
};
}
#endif
