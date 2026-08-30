#ifndef OUTSHINE_WORLD_DATA_WGS84_H
#define OUTSHINE_WORLD_DATA_WGS84_H

#include <numbers>

namespace outshine::Data {

constexpr double kWgs84A = 6378137.0;
constexpr double kWgs84F = 1.0 / 298.257223563;
constexpr double kMercatorGirthM = 2.0 * std::numbers::pi * kWgs84A;

}

#endif
