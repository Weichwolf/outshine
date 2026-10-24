#ifndef OUTSHINE_BASE_GEO_EARTH_H
#define OUTSHINE_BASE_GEO_EARTH_H

#include "Units.h"

namespace outshine {

constexpr double kContrastThresholdLn = 3.912;

constexpr double kEarthMeanRadiusM = 6371008.8;

constexpr double kStandardGravityMs2 = 9.80665;

constexpr double kIsaSeaLevelDensityKgM3 = 1.2250;

constexpr double kTilePx = 256.0;

[[nodiscard]] constexpr double CarriesFromM(double tileSpanM) noexcept {
  return tileSpanM > 0.0 ? tileSpanM / kTilePx : 0.0;
}

constexpr double kRayleighExtinctionPerKm = 0.0136;

constexpr double kMieExtinctionPerKm = 0.0444;

constexpr double kClearAirRangeM = kContrastThresholdLn / kRayleighExtinctionPerKm * kMPerKm;

constexpr double kAverageDayRangeM =
    kContrastThresholdLn / (kRayleighExtinctionPerKm + kMieExtinctionPerKm) * kMPerKm;

[[nodiscard]] constexpr double VisualRangeM(double haze) noexcept {
  const double perKm = kRayleighExtinctionPerKm + kMieExtinctionPerKm * (haze > 0.0 ? haze : 0.0);
  return kContrastThresholdLn / perKm * kMPerKm;
}

static_assert(noexcept(CarriesFromM(0.0)) && noexcept(VisualRangeM(0.0)),
              "pure visibility arithmetic does not throw");
static_assert(VisualRangeM(0.0) == kClearAirRangeM, "zero aerosol matches the model baseline");
static_assert(VisualRangeM(1.0) == kAverageDayRangeM,
              "unit haze matches the reference aerosol level");
static_assert(kClearAirRangeM > kAverageDayRangeM, "more aerosol is less sight");
static_assert(VisualRangeM(-kAverageDayRangeM) == kClearAirRangeM,
              "haze below zero is clear air rather than a negative extinction, which would read "
              "as air that ADDS contrast with distance");

struct LongitudeLatitudeHeight {

  double LongitudeDeg = 0.0;

  double LatitudeDeg = 0.0;

  double HeightM = 0.0;

  [[nodiscard]] constexpr bool operator==(const LongitudeLatitudeHeight &) const = default;
};

struct LongitudeLatitude {

  double LongitudeDeg = 0.0;

  double LatitudeDeg = 0.0;

  [[nodiscard]] constexpr bool operator==(const LongitudeLatitude &) const = default;
};

struct EastNorthUp {

  double EastM = 0.0;

  double NorthM = 0.0;

  double UpM = 0.0;

  [[nodiscard]] constexpr bool operator==(const EastNorthUp &) const = default;
};

struct EastNorth {

  double EastM = 0.0;

  double NorthM = 0.0;

  [[nodiscard]] constexpr bool operator==(const EastNorth &) const = default;
};

struct Attitude {

  double RollDeg = 0.0;

  double PitchDeg = 0.0;

  double YawDeg = 0.0;

  [[nodiscard]] constexpr bool operator==(const Attitude &) const = default;
};

struct LookDirection {

  double AzimuthDeg = 0.0;

  double ElevationDeg = 0.0;

  [[nodiscard]] constexpr bool operator==(const LookDirection &) const = default;
};

}

#endif
