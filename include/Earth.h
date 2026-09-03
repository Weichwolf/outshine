#ifndef OUTSHINE_EARTH_H
#define OUTSHINE_EARTH_H

#include "math/Units.h"

namespace outshine {

/// `-ln(0.02)`: the contrast at which the eye stops separating a dark object from the horizon.
///
/// Two per cent is the threshold Koschmieder's law is stated with and the one the WMO uses to
/// define meteorological visibility, so it is a convention of the field rather than a choice made
/// here.
constexpr double kContrastThresholdLn = 3.912;

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

/// A web-mercator tile is drawn across this many pixels, by the Slippy Map convention every tile
/// server on Earth serves.
constexpr double kTilePx = 256.0;

/// The smallest feature a tile at this span can carry, in metres.
///
/// WHY A GARDEN FENCE IS NOT VISIBLE FROM ORBIT AND THE PENTAGON IS, stated as arithmetic. A tile
/// is drawn across @ref kTilePx pixels, so anything narrower than one of those pixels cannot be
/// seen at ANY distance where this level is the one being drawn. The levels therefore form a
/// ladder, and over Zurich it reads:
///
///     z14   1668 m wide   carries from  6.5 m   houses
///     z12   6672 m wide   carries from   26 m   blocks and churches
///     z10  26689 m wide   carries from  104 m   the Great Wall
///
/// MEASURED AGAINST THE DATA IT DESCRIBES: the fifth percentile of building footprint widths over
/// Zurich is 6.47 m against the 6.51 m this returns for z14 -- two per cent apart, with nothing
/// fitted. OSM tiles at a zoom carry the geometry that zoom resolves, and this recovers the same
/// number from the projection alone.
///
/// IT SAYS WHAT A LEVEL CARRIES, NEVER WHAT MAY BE DISCARDED. On the finest level loaded nothing
/// is dropped, because no child is left to hold it and a camera may stand a centimetre from a
/// wall. Dropping happens on a COARSER level only, and only because the same feature still stands
/// on the finer one -- which is Cesium's rule for 3D Tiles.
[[nodiscard]] constexpr double CarriesFromM(double tileSpanM) {
  return tileSpanM > 0.0 ? tileSpanM / kTilePx : 0.0;
}

/// How strongly clean air scatters green light, per kilometre at sea level.
///
/// Rayleigh scattering off the gas molecules themselves, at 550 nm, from Bruneton's fit to the US
/// Standard Atmosphere. It is why the sky is blue and why a far ridge is pale blue rather than
/// pale grey, and NO WEATHER REMOVES IT -- it is the air, not something in the air.
constexpr double kRayleighExtinctionPerKm = 0.0136;

/// How strongly the AEROSOLS of an average day scatter, per kilometre at sea level.
///
/// Mie scattering off dust, smoke, salt and humidity. Unlike the gases this is weather: a hard
/// foehn morning carries a fraction of it and a summer afternoon over a city carries several
/// times as much. The figure is the average-day value the same fit states, and it is what
/// @ref Scenario::Weather::Haze scales.
constexpr double kMieExtinctionPerKm = 0.0444;

/// How far one can see through air carrying no aerosols at all, in metres.
///
/// The gases alone, and the ceiling no weather passes: on the clearest day physics allows, a dark
/// ridge stops being distinguishable at this range and not one metre further.
constexpr double kClearAirRangeM = kContrastThresholdLn / kRayleighExtinctionPerKm * kMPerKm;

/// How far one can see on the average day @ref kMieExtinctionPerKm describes, in metres.
///
/// This is why the Alps are invisible from Venice on most days: the ring reaches them at 214 km
/// and this reads a third of that.
constexpr double kAverageDayRangeM =
    kContrastThresholdLn / (kRayleighExtinctionPerKm + kMieExtinctionPerKm) * kMPerKm;

/// How far one can see through air carrying @p haze times the average day's aerosols, in metres.
///
/// KOSCHMIEDER'S LAW, which is the meteorological standard for exactly this question: a black
/// object against the horizon sky stops being distinguishable once its contrast falls to about two
/// per cent, and that happens at `3.912 / extinction`. The 3.912 is `-ln(0.02)` and not a fitted
/// number.
///
/// This is what a scenario is choosing when it declares haze, so it belongs where the declaration
/// can be read rather than inside the renderer:
///
///     haze 1.0    67 km   an average day
///     haze 0.1   217 km   a hard clear one
///     haze 0.0   288 km   the gases alone, and the ceiling no weather passes
[[nodiscard]] constexpr double VisualRangeM(double haze) {
  const double perKm = kRayleighExtinctionPerKm + kMieExtinctionPerKm * (haze > 0.0 ? haze : 0.0);
  return kContrastThresholdLn / perKm * kMPerKm;
}

static_assert(VisualRangeM(0.0) == kClearAirRangeM,
              "no aerosol is the gases alone, and that is the ceiling");
static_assert(VisualRangeM(1.0) == kAverageDayRangeM, "and one is the average day it is scaled to");
static_assert(kClearAirRangeM > kAverageDayRangeM, "more aerosol is less sight");
static_assert(VisualRangeM(-kAverageDayRangeM) == kClearAirRangeM,
              "haze below zero is clear air rather than a negative extinction, which would read "
              "as air that ADDS contrast with distance");

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
