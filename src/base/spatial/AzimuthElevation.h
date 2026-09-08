#ifndef OUTSHINE_BASE_SPATIAL_AZIMUTHELEVATION_H
#define OUTSHINE_BASE_SPATIAL_AZIMUTHELEVATION_H

#include <cmath>
#include "math/Vec3.h"

namespace outshine {
inline Vec3 EastUpSouthDirection(double azimuthRad, double elevationRad) {
  return {{std::cos(elevationRad) * std::sin(azimuthRad),
           std::sin(elevationRad),
           -std::cos(elevationRad) * std::cos(azimuthRad)}};
}
} // namespace outshine

#endif
