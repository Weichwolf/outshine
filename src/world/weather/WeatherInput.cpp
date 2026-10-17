#include "WeatherInput.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string_view>

namespace outshine {
namespace {
constexpr double kUnbounded = std::numeric_limits<double>::max();
constexpr double kMaximumLatitudeDeg = 90.0;
constexpr double kMaximumLongitudeDeg = 180.0;

bool InRange(const std::optional<double> &value, double minimum, double maximum) noexcept {
  return !value || (std::isfinite(*value) && *value >= minimum && *value <= maximum);
}

bool ValidPosition(const WeatherPosition &at) noexcept {
  return std::isfinite(at.LatitudeDeg) && std::abs(at.LatitudeDeg) <= kMaximumLatitudeDeg &&
         std::isfinite(at.LongitudeDeg) && std::abs(at.LongitudeDeg) <= kMaximumLongitudeDeg &&
         InRange(at.DeclaredSurfaceElevationAslM, -kUnbounded, kUnbounded);
}

bool ValidValues(const WeatherValues &values) noexcept {
  const std::array fractions{values.CloudTotal,
                             values.CloudLow,
                             values.CloudMid,
                             values.CloudHigh,
                             values.RelativeHumidity2m};
  for (const auto &value : fractions) {
    if (!InRange(value, 0.0, 1.0)) { return false; }
  }
  const std::array amounts{values.WindSpeed10mMs,
                           values.AirTemperature2mK,
                           values.VisibilityM,
                           values.SurfacePressurePa,
                           values.SnowDepthM,
                           values.CloudBaseAglM,
                           values.Precipitation.TotalWaterKgM2,
                           values.Precipitation.RainWaterKgM2,
                           values.Precipitation.ShowersWaterKgM2,
                           values.Precipitation.SnowfallDepthM};
  for (const auto &value : amounts) {
    if (!InRange(value, 0.0, kUnbounded)) { return false; }
  }
  constexpr double kFullBearingDeg = 360.0;
  if (!InRange(values.WindFrom10mDeg, 0.0, kFullBearingDeg) ||
      (values.WindFrom10mDeg && *values.WindFrom10mDeg == kFullBearingDeg) ||
      !InRange(values.VerticalWindMs, -kUnbounded, kUnbounded)) {
    return false;
  }
  return !values.Wind10m ||
         (std::isfinite(values.Wind10m->EastMs) && std::isfinite(values.Wind10m->NorthMs));
}

bool ValidInterval(const WeatherValues &values) noexcept {
  const auto &rain = values.Precipitation;
  const bool given = rain.TotalWaterKgM2 || rain.RainWaterKgM2 || rain.ShowersWaterKgM2 ||
                     rain.SnowfallDepthM || rain.BeginUtcS != 0 || rain.EndUtcS != 0;
  return !given || (rain.BeginUtcS < rain.EndUtcS && rain.EndUtcS <= values.ValidUtcS);
}
}

std::expected<void, WeatherInputFailure> ValidateWeatherInput(const WeatherPosition &at,
                                                              const WeatherValues &values,
                                                              int64_t selectedUtcS) noexcept {
  if (values.ValidUtcS != selectedUtcS) { return std::unexpected(WeatherInputFailure::InvalidUtc); }
  if (!ValidPosition(at)) { return std::unexpected(WeatherInputFailure::InvalidPosition); }
  if (!ValidValues(values)) { return std::unexpected(WeatherInputFailure::InvalidValues); }
  if (!ValidInterval(values)) { return std::unexpected(WeatherInputFailure::InvalidInterval); }
  return {};
}

std::string_view WeatherInputMessage(WeatherInputFailure failure) noexcept {
  switch (failure) {
    case WeatherInputFailure::InvalidPosition: return "weather requires a finite geodetic position";
    case WeatherInputFailure::InvalidValues:
      return "weather values are outside their native ranges";
    case WeatherInputFailure::InvalidUtc: return "weather validity must equal the selected UTC";
    case WeatherInputFailure::InvalidInterval:
      return "weather amounts require a preceding nonempty interval";
  }
  return "invalid native weather input";
}
}
