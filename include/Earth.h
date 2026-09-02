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

/// A place in the local horizontal frame of some origin, in metres.
///
/// East and north span the tangent plane and up leaves it. Cesium calls the frame a local
/// horizontal coordinate system; this is a point inside one.
struct EastNorthUp {
  /// Toward the east, in metres.
  double EastM = 0.0;
  /// Toward the north, in metres.
  double NorthM = 0.0;
  /// Away from the ellipsoid, in metres.
  double UpM = 0.0;

  /// Two offsets are the same offset when all three measures are.
  [[nodiscard]] constexpr bool operator==(const EastNorthUp &) const = default;
};

/// A place in the local horizontal PLANE, in metres.
///
/// Kept apart from @ref EastNorthUp rather than folded into it: a building outline is stored in
/// bulk and a third measure it never uses would make every one of them half again as large.
struct EastNorth {
  /// Toward the east, in metres.
  double EastM = 0.0;
  /// Toward the north, in metres.
  double NorthM = 0.0;

  /// Two points are the same point when both measures are.
  [[nodiscard]] constexpr bool operator==(const EastNorth &) const = default;
};

/// How a body stands in its local horizontal frame, in degrees.
///
/// Unreal calls this an FRotator and RAGE carries the same three angles; the order they are
/// APPLIED is yaw, then pitch, then roll, and the field order here says nothing about that -- the
/// function that consumes them does.
struct Attitude {
  /// Rotation about the forward axis, positive right wing down.
  double RollDeg = 0.0;
  /// Rotation about the right axis, positive nose up.
  double PitchDeg = 0.0;
  /// Rotation about the up axis, positive turning east from north.
  double YawDeg = 0.0;

  /// Two attitudes are the same attitude when all three angles are.
  [[nodiscard]] constexpr bool operator==(const Attitude &) const = default;
};

/// A direction relative to a body, in degrees.
///
/// Azimuth turns right from the body's nose and elevation lifts from its horizontal -- what a
/// sensor reports and what a scenario declares when it aims one.
struct LookDirection {
  /// Right of the nose, in degrees.
  double AzimuthDeg = 0.0;
  /// Above the horizontal, in degrees.
  double ElevationDeg = 0.0;

  /// Two directions are the same direction when both angles are.
  [[nodiscard]] constexpr bool operator==(const LookDirection &) const = default;
};

} // namespace outshine

#endif
