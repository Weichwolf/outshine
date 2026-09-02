#ifndef OUTSHINE_EARTH_H
#define OUTSHINE_EARTH_H

namespace outshine {

/// The mean radius of the WGS84 ellipsoid, in metres.
///
/// It is the radius a sphere would need to have the ellipsoid's volume, which is what a scenario
/// means when it declares a world radius without declaring an ellipsoid.
constexpr double kEarthMeanRadiusM = 6371008.8;

/// Standard gravity at the surface, in metres per second squared, as ISO 80000-3 fixes it.
///
/// A scenario may declare its own -- a body on a different world, or a deliberately light one --
/// and this is what stands when it declares none.
constexpr double kStandardGravityMs2 = 9.80665;

/// Air density at sea level in the International Standard Atmosphere, in kilograms per cubic metre.
constexpr double kIsaSeaLevelDensityKgM3 = 1.2250;

} // namespace outshine

#endif
