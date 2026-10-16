#ifndef OUTSHINE_SCENARIO_NATIVEWEATHERFIELDS_H
#define OUTSHINE_SCENARIO_NATIVEWEATHERFIELDS_H

#include <world/weather/WeatherSnapshot.h>
#include <array>
#include <optional>

namespace outshine {
struct NativeWeatherField {
  const char *Name;
  std::optional<double> WeatherValues::*Member;
};

inline constexpr std::array kNativeWeatherFields{
    NativeWeatherField{.Name = "cloudTotal", .Member = &WeatherValues::CloudTotal},
    NativeWeatherField{.Name = "cloudLow", .Member = &WeatherValues::CloudLow},
    NativeWeatherField{.Name = "cloudMid", .Member = &WeatherValues::CloudMid},
    NativeWeatherField{.Name = "cloudHigh", .Member = &WeatherValues::CloudHigh},
    NativeWeatherField{.Name = "windSpeed10mMs", .Member = &WeatherValues::WindSpeed10mMs},
    NativeWeatherField{.Name = "windFrom10mDeg", .Member = &WeatherValues::WindFrom10mDeg},
    NativeWeatherField{.Name = "airTemperature2mK", .Member = &WeatherValues::AirTemperature2mK},
    NativeWeatherField{.Name = "relativeHumidity2m", .Member = &WeatherValues::RelativeHumidity2m},
    NativeWeatherField{.Name = "visibilityM", .Member = &WeatherValues::VisibilityM},
    NativeWeatherField{.Name = "surfacePressurePa", .Member = &WeatherValues::SurfacePressurePa},
    NativeWeatherField{.Name = "snowDepthM", .Member = &WeatherValues::SnowDepthM},
    NativeWeatherField{.Name = "cloudBaseAglM", .Member = &WeatherValues::CloudBaseAglM},
    NativeWeatherField{.Name = "verticalWindMs", .Member = &WeatherValues::VerticalWindMs}};

struct NativePrecipitationField {
  const char *Name;
  std::optional<double> WeatherPrecipitation::*Member;
};

inline constexpr std::array kNativePrecipitationFields{
    NativePrecipitationField{.Name = "totalWaterKgM2",
                             .Member = &WeatherPrecipitation::TotalWaterKgM2},
    NativePrecipitationField{.Name = "rainWaterKgM2",
                             .Member = &WeatherPrecipitation::RainWaterKgM2},
    NativePrecipitationField{.Name = "showersWaterKgM2",
                             .Member = &WeatherPrecipitation::ShowersWaterKgM2},
    NativePrecipitationField{.Name = "snowfallDepthM",
                             .Member = &WeatherPrecipitation::SnowfallDepthM}};
}
#endif
