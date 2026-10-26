#include "OpenMeteo.h"
#include "OpenMeteoFields.h"
#include "format/Json.h"
#include "format/Sha256.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <expected>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

namespace outshine::Generators::Weather {
namespace {
constexpr uint16_t kHttpOk = 200;
constexpr double kLatitudeLimitDeg = 90.0;
constexpr double kLongitudeLimitDeg = 180.0;

bool ValidSource(const ForecastSource &source) noexcept {
  const auto &at = source.Requested;
  return source.HttpStatus == kHttpOk &&
         source.RequestUri.starts_with("https://api.open-meteo.com/v1/forecast?") &&
         source.PayloadSha256.size() == 64 && std::isfinite(at.LatitudeDeg) &&
         at.LatitudeDeg >= -kLatitudeLimitDeg && at.LatitudeDeg <= kLatitudeLimitDeg &&
         std::isfinite(at.LongitudeDeg) && at.LongitudeDeg >= -kLongitudeLimitDeg &&
         at.LongitudeDeg <= kLongitudeLimitDeg &&
         (!at.DeclaredSurfaceElevationAslM || std::isfinite(*at.DeclaredSurfaceElevationAslM));
}

bool Truthy(Json::Ref value) {
  switch (value.GetKind()) {
    case Json::Kind::Invalid:
    case Json::Kind::Null: return false;
    case Json::Kind::Bool: return value.Bool();
    case Json::Kind::Number: return value.Num() != 0.0;
    case Json::Kind::String: return !value.Str().empty();
    case Json::Kind::Object:
    case Json::Kind::Array: return value.Size() != 0;
  }
  return false;
}

std::expected<void, ForecastError> Times(Json::Ref hourly, Forecast &forecast) {
  const auto times = hourly["time"];
  if (times.GetKind() != Json::Kind::Array || times.Size() == 0 ||
      times.Size() > kOpenMeteoMaximumHours) {
    return std::unexpected(
        ForecastError{.Reason = ForecastFailure::InvalidCount, .Field = "hourly.time"});
  }
  forecast.Hours.resize(times.Size());
  for (size_t index = 0; index < times.Size(); ++index) {
    const auto parsed = WeatherInteger(times[index], "hourly.time");
    if (!parsed || *parsed < std::numeric_limits<int64_t>::min() + kWeatherHourS) {
      return std::unexpected(ForecastError{
          .Reason = ForecastFailure::InvalidTime, .Field = "hourly.time", .Index = index});
    }
    if (index > 0) {
      const int64_t previous = forecast.Hours[index - 1].ValidUtcS;
      if (previous > std::numeric_limits<int64_t>::max() - kWeatherHourS ||
          *parsed != previous + kWeatherHourS) {
        return std::unexpected(ForecastError{
            .Reason = ForecastFailure::InvalidTime, .Field = "hourly.time", .Index = index});
      }
    }
    auto &hour = forecast.Hours[index];
    hour.ValidUtcS = *parsed;
    hour.Precipitation.BeginUtcS = *parsed - kWeatherHourS;
    hour.Precipitation.EndUtcS = *parsed;
  }
  return {};
}

std::expected<void, ForecastError> Position(Json::Ref root, Forecast &forecast) {
  constexpr double unbounded = std::numeric_limits<double>::infinity();
  const auto lat =
      OptionalWeatherNumber(root["latitude"], "latitude", -kLatitudeLimitDeg, kLatitudeLimitDeg);
  const auto lon = OptionalWeatherNumber(
      root["longitude"], "longitude", -kLongitudeLimitDeg, kLongitudeLimitDeg);
  const auto elevation =
      OptionalWeatherNumber(root["elevation"], "elevation", -unbounded, unbounded);
  if (!lat) { return std::unexpected(lat.error()); }
  if (!lon) { return std::unexpected(lon.error()); }
  if (!elevation) { return std::unexpected(elevation.error()); }
  forecast.ModelGridLatitudeDeg = *lat;
  forecast.ModelGridLongitudeDeg = *lon;
  forecast.DownscalingSurfaceElevationAslM = *elevation;
  const auto offset = root["utc_offset_seconds"];
  if (offset.Valid() && offset.GetKind() != Json::Kind::Null) {
    const auto parsed = WeatherInteger(offset, "utc_offset_seconds");
    if (!parsed) { return std::unexpected(parsed.error()); }
    forecast.SourceUtcOffsetS = *parsed;
  }
  return {};
}
}

std::expected<int64_t, ForecastError> WeatherInteger(Json::Ref value, const char *field) {
  if (value.GetKind() != Json::Kind::Number) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InvalidValue, .Field = field});
  }
  int64_t result = 0;
  const auto text = value.Source();
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InvalidValue, .Field = field});
  }
  return result;
}

std::expected<std::optional<double>, ForecastError>
OptionalWeatherNumber(Json::Ref value, const char *field, double minimum, double maximum) {
  if (!value.Valid() || value.GetKind() == Json::Kind::Null) { return std::optional<double>{}; }
  const double number = value.Num();
  if (value.GetKind() != Json::Kind::Number || !std::isfinite(number) || number < minimum ||
      number > maximum) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InvalidValue, .Field = field});
  }
  return number;
}

std::expected<Forecast, ForecastError> DecodeOpenMeteo(std::span<const uint8_t> bytes,
                                                       const ForecastSource &source) {
  if (bytes.size() > kOpenMeteoMaximumBytes) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InputTooLarge});
  }
  if (!ValidSource(source)) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InvalidSource});
  }
  if (Sha256Hex(bytes.data(), bytes.size()) != source.PayloadSha256) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::HashMismatch});
  }
  Json doc;
  if (!doc.Parse(reinterpret_cast<const char *>(bytes.data()), bytes.size())) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InvalidJson});
  }
  const auto root = doc.Root();
  if (root.GetKind() != Json::Kind::Object) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InvalidStructure});
  }
  if (Truthy(root["error"])) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::ProviderError});
  }
  const auto hourly = root["hourly"];
  const auto units = root["hourly_units"];
  if (hourly.GetKind() != Json::Kind::Object || units.GetKind() != Json::Kind::Object) {
    return std::unexpected(
        ForecastError{.Reason = ForecastFailure::InvalidStructure, .Field = "hourly"});
  }
  if (!units["time"].StrEquals("unixtime")) {
    return std::unexpected(ForecastError{.Reason = ForecastFailure::InvalidUnits, .Field = "time"});
  }
  Forecast forecast;
  forecast.Source = source;
  if (const auto result = Times(hourly, forecast); !result) {
    return std::unexpected(result.error());
  }
  if (const auto result = Position(root, forecast); !result) {
    return std::unexpected(result.error());
  }
  if (const auto result = DecodeWeatherColumns(hourly, units, forecast); !result) {
    return std::unexpected(result.error());
  }
  return forecast;
}

const HourlyWeather *ExactHour(const Forecast &forecast, int64_t utcS) noexcept {
  const auto found = std::ranges::lower_bound(forecast.Hours, utcS, {}, &HourlyWeather::ValidUtcS);
  return found != forecast.Hours.end() && found->ValidUtcS == utcS ? &*found : nullptr;
}
}
