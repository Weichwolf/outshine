#ifndef OUTSHINE_WORLD_DATA_WGS84_H
#define OUTSHINE_WORLD_DATA_WGS84_H

#include <numbers>

#include "math/Units.h"

namespace outshine::Data {

constexpr double kWgs84A = outshine::kWgs84A;
constexpr double kWgs84F = 1.0 / 298.257223563;
constexpr double kMercatorGirthM = 2.0 * std::numbers::pi * kWgs84A;

} // namespace outshine::Data

#endif
