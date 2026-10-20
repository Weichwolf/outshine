#ifndef OUTSHINE_WORLD_DATA_WGS84_H
#define OUTSHINE_WORLD_DATA_WGS84_H

#include <numbers>

#include "math/Units.h"

namespace outshine::Data {

using outshine::kWgs84F;
constexpr double kMercatorGirthM = 2.0 * std::numbers::pi * outshine::kWgs84A;

}

#endif
