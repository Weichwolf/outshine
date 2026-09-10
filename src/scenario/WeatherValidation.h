#ifndef OUTSHINE_SCENARIO_WEATHERVALIDATION_H
#define OUTSHINE_SCENARIO_WEATHERVALIDATION_H

#include <scenario/Scenario.h>
#include <array>
#include <cmath>
#include <expected>
#include <limits>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view InvalidWeather =
    "weather requires finite numbers within the declared physical and storage ranges";
}

struct WeatherField {
  const char *Name;
  double Scenario::Weather::*Member;
  double Minimum = 0.0;
  double Maximum = std::numeric_limits<double>::max();
};

inline constexpr std::array kWeatherFields{
    WeatherField{.Name = "cloudCover", .Member = &Scenario::Weather::CloudCover, .Maximum = 1.0},
    WeatherField{.Name = "cloudLow", .Member = &Scenario::Weather::CloudLow, .Maximum = 1.0},
    WeatherField{.Name = "cloudMid", .Member = &Scenario::Weather::CloudMid, .Maximum = 1.0},
    WeatherField{.Name = "cloudHigh", .Member = &Scenario::Weather::CloudHigh, .Maximum = 1.0},
    WeatherField{.Name = "cloudBaseAglM", .Member = &Scenario::Weather::CloudBaseAglM},
    WeatherField{.Name = "windDeg",
                 .Member = &Scenario::Weather::WindDeg,
                 .Minimum = -std::numeric_limits<double>::max()},
    WeatherField{.Name = "windMs", .Member = &Scenario::Weather::WindMs},
    WeatherField{.Name = "haze",
                 .Member = &Scenario::Weather::Haze,
                 .Maximum = std::numeric_limits<float>::max()}};

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateWeather(const Scenario::Weather &weather) noexcept {
  for (const auto &field : kWeatherFields) {
    const double value = weather.*field.Member;
    if (!std::isfinite(value) || value < field.Minimum || value > field.Maximum) {
      return std::unexpected(Says::InvalidWeather);
    }
  }
  return {};
}
}
#endif
