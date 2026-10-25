#ifndef OUTSHINE_BASE_GEO_WGS84_H
#define OUTSHINE_BASE_GEO_WGS84_H

#include <numbers>

#include "Units.h"

namespace outshine::Data {

using outshine::kWgs84F;
constexpr double kMercatorGirthM = 2.0 * std::numbers::pi * outshine::kWgs84A;

}

#endif
