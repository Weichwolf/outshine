#ifndef OUTSHINE_SOURCES_WEATHER_WEATHERSNAPSHOT_H
#define OUTSHINE_SOURCES_WEATHER_WEATHERSNAPSHOT_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace outshine {

struct WeatherPosition {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  std::optional<double> DeclaredSurfaceElevationAslM = std::nullopt;
};

struct WeatherWind10m {
  double EastMs = 0.0;
  double NorthMs = 0.0;
};

struct WeatherPrecipitation {
  int64_t BeginUtcS = 0;
  int64_t EndUtcS = 0;
  std::optional<double> TotalWaterKgM2 = std::nullopt;
  std::optional<double> RainWaterKgM2 = std::nullopt;
  std::optional<double> ShowersWaterKgM2 = std::nullopt;
  std::optional<double> SnowfallDepthM = std::nullopt;
};

struct WeatherValues {
  int64_t ValidUtcS = 0;
  std::optional<double> CloudTotal = std::nullopt;
  std::optional<double> CloudLow = std::nullopt;
  std::optional<double> CloudMid = std::nullopt;
  std::optional<double> CloudHigh = std::nullopt;
  std::optional<double> WindSpeed10mMs = std::nullopt;
  std::optional<double> WindFrom10mDeg = std::nullopt;
  std::optional<WeatherWind10m> Wind10m = std::nullopt;
  std::optional<double> AirTemperature2mK = std::nullopt;
  std::optional<double> RelativeHumidity2m = std::nullopt;
  std::optional<double> VisibilityM = std::nullopt;
  std::optional<double> SurfacePressurePa = std::nullopt;
  std::optional<double> SnowDepthM = std::nullopt;
  std::optional<double> CloudBaseAglM = std::nullopt;
  std::optional<double> VerticalWindMs = std::nullopt;
  WeatherPrecipitation Precipitation{};
};

enum class WeatherOrigin : uint8_t { Unknown, ModelForecast, ScenarioDeclaration };

struct WeatherProvenance {
  WeatherOrigin Origin = WeatherOrigin::Unknown;
  std::string SourceId;
  std::string SourceRevision;
  std::string Provider;
  WeatherPosition Requested{};
  std::optional<double> ModelGridLatitudeDeg = std::nullopt;
  std::optional<double> ModelGridLongitudeDeg = std::nullopt;
  std::optional<double> DownscalingSurfaceElevationAslM = std::nullopt;
  std::optional<int64_t> SourceUtcOffsetS = std::nullopt;
  std::string RequestUri;
  std::string PayloadSha256;
  std::optional<int64_t> AcquiredUtcS = std::nullopt;
  std::optional<int64_t> ModelRunUtcS = std::nullopt;
  std::optional<std::vector<std::string>> RequestedModels = std::nullopt;
};

struct WeatherSnapshot {
  WeatherValues Values{};
  WeatherProvenance Source{};
};

}
#endif
