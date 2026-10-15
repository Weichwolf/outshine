#ifndef OUTSHINE_GENERATORS_WEATHER_FORECAST_H
#define OUTSHINE_GENERATORS_WEATHER_FORECAST_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <world/weather/WeatherSnapshot.h>

namespace outshine::Generators::Weather {

using RequestedPosition = WeatherPosition;

struct ForecastSource {
  RequestedPosition Requested;
  std::string RequestUri;
  std::string PayloadSha256;
  std::optional<int64_t> AcquiredUtcS;
  std::optional<int64_t> ModelRunUtcS;
  std::optional<std::vector<std::string>> RequestedModels;
  uint16_t HttpStatus = 0;
};

using HorizontalWind10m = WeatherWind10m;
using PreviousHour = WeatherPrecipitation;
using HourlyWeather = WeatherValues;

struct Forecast {
  ForecastSource Source;
  std::optional<double> ModelGridLatitudeDeg;
  std::optional<double> ModelGridLongitudeDeg;
  std::optional<double> DownscalingSurfaceElevationAslM;
  std::optional<int64_t> SourceUtcOffsetS;
  std::vector<HourlyWeather> Hours;
};

[[nodiscard]] const HourlyWeather *ExactHour(const Forecast &forecast, int64_t utcS) noexcept;

}
#endif
