#ifndef OUTSHINE_GENERATORS_WEATHER_FORECAST_H
#define OUTSHINE_GENERATORS_WEATHER_FORECAST_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Generators::Weather {

struct RequestedPosition {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  std::optional<double> DeclaredSurfaceElevationAslM;
};

struct ForecastSource {
  RequestedPosition Requested;
  std::string RequestUri;
  std::string PayloadSha256;
  std::optional<int64_t> AcquiredUtcS;
  std::optional<int64_t> ModelRunUtcS;
  std::optional<std::vector<std::string>> RequestedModels;
  uint16_t HttpStatus = 0;
};

struct HorizontalWind10m {
  double EastMs = 0.0;
  double NorthMs = 0.0;
};

struct PreviousHour {
  int64_t BeginUtcS = 0;
  int64_t EndUtcS = 0;
  std::optional<double> TotalWaterKgM2;
  std::optional<double> RainWaterKgM2;
  std::optional<double> ShowersWaterKgM2;
  std::optional<double> SnowfallDepthM;
};

struct HourlyWeather {
  int64_t ValidUtcS = 0;
  std::optional<double> CloudTotal;
  std::optional<double> CloudLow;
  std::optional<double> CloudMid;
  std::optional<double> CloudHigh;
  std::optional<double> WindSpeed10mMs;
  std::optional<double> WindFrom10mDeg;
  std::optional<HorizontalWind10m> Wind10m;
  std::optional<double> AirTemperature2mK;
  std::optional<double> RelativeHumidity2m;
  std::optional<double> VisibilityM;
  std::optional<double> SurfacePressurePa;
  std::optional<double> SnowDepthM;
  std::optional<double> CloudBaseAglM;
  std::optional<double> VerticalWindMs;
  PreviousHour Precipitation;
};

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
