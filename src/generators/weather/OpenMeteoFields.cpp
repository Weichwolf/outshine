#include "OpenMeteoFields.h"

#include <array>
#include <cmath>
#include <expected>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>

namespace outshine::Generators::Weather {
namespace {
constexpr double kFullCircleDeg = 360.0;
constexpr double kHalfCircleDeg = kFullCircleDeg / 2.0;
constexpr double kUnbounded = std::numeric_limits<double>::infinity();

template <typename Target> struct Field {
  const char *Name;
  const char *Unit;
  std::optional<double> Target::*Destination;
  double Scale = 1.0;
  double Offset = 0.0;
  double Minimum = 0.0;
  double Maximum = kUnbounded;
};

constexpr std::array kInstantFields = {
    Field<HourlyWeather>{.Name = "cloud_cover",
                         .Unit = "%",
                         .Destination = &HourlyWeather::CloudTotal,
                         .Scale = 0.01,
                         .Offset = 0.0,
                         .Minimum = 0.0,
                         .Maximum = 1.0},
    Field<HourlyWeather>{.Name = "cloud_cover_low",
                         .Unit = "%",
                         .Destination = &HourlyWeather::CloudLow,
                         .Scale = 0.01,
                         .Offset = 0.0,
                         .Minimum = 0.0,
                         .Maximum = 1.0},
    Field<HourlyWeather>{.Name = "cloud_cover_mid",
                         .Unit = "%",
                         .Destination = &HourlyWeather::CloudMid,
                         .Scale = 0.01,
                         .Offset = 0.0,
                         .Minimum = 0.0,
                         .Maximum = 1.0},
    Field<HourlyWeather>{.Name = "cloud_cover_high",
                         .Unit = "%",
                         .Destination = &HourlyWeather::CloudHigh,
                         .Scale = 0.01,
                         .Offset = 0.0,
                         .Minimum = 0.0,
                         .Maximum = 1.0},
    Field<HourlyWeather>{
        .Name = "wind_speed_10m", .Unit = "m/s", .Destination = &HourlyWeather::WindSpeed10mMs},
    Field<HourlyWeather>{.Name = "wind_direction_10m",
                         .Unit = "°",
                         .Destination = &HourlyWeather::WindFrom10mDeg,
                         .Scale = 1.0,
                         .Offset = 0.0,
                         .Minimum = 0.0,
                         .Maximum = 360.0},
    Field<HourlyWeather>{.Name = "temperature_2m",
                         .Unit = "°C",
                         .Destination = &HourlyWeather::AirTemperature2mK,
                         .Scale = 1.0,
                         .Offset = 273.15},
    Field<HourlyWeather>{.Name = "relative_humidity_2m",
                         .Unit = "%",
                         .Destination = &HourlyWeather::RelativeHumidity2m,
                         .Scale = 0.01,
                         .Offset = 0.0,
                         .Minimum = 0.0,
                         .Maximum = 1.0},
    Field<HourlyWeather>{
        .Name = "visibility", .Unit = "m", .Destination = &HourlyWeather::VisibilityM},
    Field<HourlyWeather>{.Name = "surface_pressure",
                         .Unit = "hPa",
                         .Destination = &HourlyWeather::SurfacePressurePa,
                         .Scale = 100.0},
    Field<HourlyWeather>{
        .Name = "snow_depth", .Unit = "m", .Destination = &HourlyWeather::SnowDepthM},
};

constexpr std::array kAmountFields = {
    Field<PreviousHour>{
        .Name = "precipitation", .Unit = "mm", .Destination = &PreviousHour::TotalWaterKgM2},
    Field<PreviousHour>{.Name = "rain", .Unit = "mm", .Destination = &PreviousHour::RainWaterKgM2},
    Field<PreviousHour>{
        .Name = "showers", .Unit = "mm", .Destination = &PreviousHour::ShowersWaterKgM2},
    Field<PreviousHour>{.Name = "snowfall",
                        .Unit = "cm",
                        .Destination = &PreviousHour::SnowfallDepthM,
                        .Scale = 0.01},
};

template <typename Target, typename Select>
std::expected<void, ForecastError> Column(Json::Ref hourly,
                                          const Field<Target> &field,
                                          Json::Ref units,
                                          Forecast &forecast,
                                          Select select) {
  const auto values = hourly[field.Name];
  if (!values.Valid()) { return {}; }
  if (values.GetKind() != Json::Kind::Array || values.Size() != forecast.Hours.size()) {
    return std::unexpected(ForecastError{ForecastFailure::InvalidCount, field.Name});
  }
  const bool unitValid = units[field.Name].StrEquals(field.Unit);
  for (size_t index = 0; index < values.Size(); ++index) {
    const auto value = values[index];
    if (value.GetKind() == Json::Kind::Null) { continue; }
    if (!unitValid) {
      return std::unexpected(ForecastError{ForecastFailure::InvalidUnits, field.Name, index});
    }
    if (value.GetKind() != Json::Kind::Number) {
      return std::unexpected(ForecastError{ForecastFailure::InvalidValue, field.Name, index});
    }
    const double converted = value.Num() * field.Scale + field.Offset;
    if (!std::isfinite(converted) || converted < field.Minimum || converted > field.Maximum) {
      return std::unexpected(ForecastError{ForecastFailure::InvalidValue, field.Name, index});
    }
    select(forecast.Hours[index]).*field.Destination = converted;
  }
  return {};
}

void Wind(HourlyWeather &hour) noexcept {
  if (hour.WindFrom10mDeg) {
    *hour.WindFrom10mDeg = std::fmod(*hour.WindFrom10mDeg, kFullCircleDeg);
  }
  if (!hour.WindSpeed10mMs) { return; }
  if (*hour.WindSpeed10mMs == 0.0) {
    hour.Wind10m = HorizontalWind10m{};
  } else if (hour.WindFrom10mDeg) {
    const double angle = *hour.WindFrom10mDeg * std::numbers::pi / kHalfCircleDeg;
    hour.Wind10m = HorizontalWind10m{.EastMs = -*hour.WindSpeed10mMs * std::sin(angle),
                                     .SouthMs = *hour.WindSpeed10mMs * std::cos(angle)};
  }
}
}

std::expected<void, ForecastError>
DecodeWeatherColumns(Json::Ref hourly, Json::Ref units, Forecast &forecast) {
  for (const auto &field : kInstantFields) {
    auto result =
        Column(hourly, field, units, forecast, [](HourlyWeather &hour) -> auto & { return hour; });
    if (!result) { return result; }
  }
  for (const auto &field : kAmountFields) {
    auto result = Column(hourly, field, units, forecast, [](HourlyWeather &hour) -> auto & {
      return hour.Precipitation;
    });
    if (!result) { return result; }
  }
  for (auto &hour : forecast.Hours) { Wind(hour); }
  return {};
}
}
