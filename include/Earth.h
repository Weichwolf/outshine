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

/// A place on the ellipsoid.
///
/// Cesium spells it this way -- `LongitudeLatitudeHeight`, longitude first -- and this tree had it
/// twice under two names before board:2093 put it here: `Ground::Geo` in the tiling and
/// `Scenario::LongitudeLatitudeHeight` in the declaration schema. A geodetic pair passed as two
/// bare doubles is a pair somebody eventually passes the other way round, which is what makes this
/// a TYPE rather than a convention.
struct LongitudeLatitudeHeight {
  /// East of Greenwich, in degrees.
  double LongitudeDeg = 0.0;
  /// North of the equator, in degrees.
  double LatitudeDeg = 0.0;
  /// Above the ellipsoid, in metres. Zero where a caller means the surface.
  double HeightM = 0.0;

  /// Two places are the same place when all three measures are.
  [[nodiscard]] constexpr bool operator==(const LongitudeLatitudeHeight &) const = default;
};

} // namespace outshine

#endif
