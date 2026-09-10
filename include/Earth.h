#ifndef OUTSHINE_EARTH_H
#define OUTSHINE_EARTH_H

#include "math/Units.h"

namespace outshine {

/// Rounded -ln(0.02), the chosen 2% contrast threshold for the uniform-extinction model.
/// This is a model convention, not a universal human-visibility cutoff.
constexpr double kContrastThresholdLn = 3.912;

/// The mean radius of the WGS84 ellipsoid, in metres.
///
/// Arithmetic mean (2a + b) / 3, rounded to 0.1 m; not the equal-volume radius.
constexpr double kEarthMeanRadiusM = 6371008.8;

/// Standard gravity at the surface, in metres per second squared, as ISO 80000-3 fixes it.
///
/// A scenario may declare its own -- a body on a different world, or a deliberately light one --
/// and this is what stands when it declares none.
constexpr double kStandardGravityMs2 = 9.80665;

/// Air density at sea level in the International Standard Atmosphere, in kilograms per cubic metre.
constexpr double kIsaSeaLevelDensityKgM3 = 1.2250;

/// Nominal 256-pixel tile width used by the sampling heuristic; providers may differ.
/// Vector-tile extent and geometric detail are separate quantities.
constexpr double kTilePx = 256.0;

/// Nominal sample spacing for a tile represented by 256 samples across its span.
/// @param tileSpanM Tile width in metres; finite values are expected.
/// @return tileSpanM / 256 for positive input, otherwise zero; NaN also produces zero.
/// This heuristic does not prove which OSM features exist or can safely be discarded.
/// No allocation, ownership transfer or coordinate conversion; safe for concurrent calls.
[[nodiscard]] constexpr double CarriesFromM(double tileSpanM) {
  return tileSpanM > 0.0 ? tileSpanM / kTilePx : 0.0;
}

/// Reference sea-level molecular extinction coefficient near green wavelengths, in km^-1.
/// Fixed input to the simplified visibility model, not a complete spectral atmosphere.
constexpr double kRayleighExtinctionPerKm = 0.0136;

/// Reference aerosol extinction coefficient in km^-1, scaled by the model's haze factor.
/// Haze 1 selects this reference; it does not imply a measured average at a given place/time.
constexpr double kMieExtinctionPerKm = 0.0444;

/// Model contrast range in metres with zero aerosol contribution and uniform extinction.
/// Excludes terrain occlusion, Earth curvature and observer/object conditions.
constexpr double kClearAirRangeM = kContrastThresholdLn / kRayleighExtinctionPerKm * kMPerKm;

/// Model contrast range in metres at the reference aerosol level, haze 1.
constexpr double kAverageDayRangeM =
    kContrastThresholdLn / (kRayleighExtinctionPerKm + kMieExtinctionPerKm) * kMPerKm;

/// Approximate contrast range from 3.912 / uniform extinction, converted from km to metres.
/// @param haze Dimensionless aerosol multiplier. Positive values scale the reference aerosol
///             coefficient; nonpositive values and NaN select zero aerosol contribution.
/// @return Model range in metres; positive infinity yields zero. This is not a geometric
///         horizon, actual weather observation or universal physical visibility limit.
/// No allocation or mutation; safe for concurrent calls. Molecular extinction remains fixed.
[[nodiscard]] constexpr double VisualRangeM(double haze) {
  const double perKm = kRayleighExtinctionPerKm + kMieExtinctionPerKm * (haze > 0.0 ? haze : 0.0);
  return kContrastThresholdLn / perKm * kMPerKm;
}

static_assert(VisualRangeM(0.0) == kClearAirRangeM, "zero aerosol matches the model baseline");
static_assert(VisualRangeM(1.0) == kAverageDayRangeM,
              "unit haze matches the reference aerosol level");
static_assert(kClearAirRangeM > kAverageDayRangeM, "more aerosol is less sight");
static_assert(VisualRangeM(-kAverageDayRangeM) == kClearAirRangeM,
              "haze below zero is clear air rather than a negative extinction, which would read "
              "as air that ADDS contrast with distance");

/// A place on the ellipsoid.
///
/// Longitude first, angles in degrees. Cesium Cartographic stores radians; convert at that
/// boundary.
struct LongitudeLatitudeHeight {
  /// East of Greenwich, in degrees.
  double LongitudeDeg = 0.0;
  /// North of the equator, in degrees.
  double LatitudeDeg = 0.0;
  /// Above the WGS84 ellipsoid, in metres; zero is the ellipsoid, not terrain or sea level.
  double HeightM = 0.0;

  /// Two places are the same place when all three measures are.
  /// @return Exact component equality, without tolerance, angular wrapping or validation.
  [[nodiscard]] constexpr bool operator==(const LongitudeLatitudeHeight &) const = default;
};

/// A place on the ellipsoid with NO height, because some questions do not have one.
///
/// A tile scheme and a map projection carry a longitude and a latitude and nothing else; handing
/// them a `LongitudeLatitudeHeight` means inventing a zero and then hoping nobody reads it. Cesium
/// keeps the same distinction -- `Cartographic` is the three, `GlobeRectangle`'s corners are the
/// two.
struct LongitudeLatitude {
  /// East of Greenwich, in degrees.
  double LongitudeDeg = 0.0;
  /// North of the equator, in degrees.
  double LatitudeDeg = 0.0;

  /// Two places are the same place when both measures are.
  /// @return Exact component equality, without tolerance, angular wrapping or validation.
  [[nodiscard]] constexpr bool operator==(const LongitudeLatitude &) const = default;
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
  /// @return Exact component equality, without tolerance, angular wrapping or validation.
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
  /// @return Exact component equality, without tolerance, angular wrapping or validation.
  [[nodiscard]] constexpr bool operator==(const EastNorth &) const = default;
};

/// Euler attitude in degrees for the local body/geodetic conversion routines.
/// Yaw selects heading clockwise from north; pitch raises the nose; roll lowers the right side.
/// Angles are stored without wrapping or validation. Composition convention belongs to the
/// consuming transformation; these values are not a quaternion or an engine-independent matrix.
struct Attitude {
  /// Rotation about the forward axis, positive right wing down.
  double RollDeg = 0.0;
  /// Rotation about the right axis, positive nose up.
  double PitchDeg = 0.0;
  /// Rotation about the up axis, positive turning east from north.
  double YawDeg = 0.0;

  /// Two attitudes are the same attitude when all three angles are.
  /// @return Exact component equality, without tolerance, angular wrapping or validation.
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
  /// @return Exact component equality, without tolerance, angular wrapping or validation.
  [[nodiscard]] constexpr bool operator==(const LookDirection &) const = default;
};

}

#endif
