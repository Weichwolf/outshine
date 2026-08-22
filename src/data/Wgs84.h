#ifndef OUTSHINE_DATA_WGS84_H
#define OUTSHINE_DATA_WGS84_H

#include <numbers>

namespace outshine::Data {

// the shape of the shipped providers' data -- OSM, Terrarium and the Web-Mercator tile
// pyramid address THIS ellipsoid; a provider for another sphere declares its own grid
constexpr double kWgs84A = 6378137.0;
constexpr double kWgs84F = 1.0 / 298.257223563;
constexpr double kMercatorGirthM = 2.0 * std::numbers::pi * kWgs84A;

}

#endif
