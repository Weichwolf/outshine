#ifndef OUTSHINE_WORLD_WEATHER_WEATHERINPUT_H
#define OUTSHINE_WORLD_WEATHER_WEATHERINPUT_H

#include <world/weather/WeatherSnapshot.h>
#include <cstdint>
#include <expected>
#include <string_view>

namespace outshine {
enum class WeatherInputFailure { InvalidPosition, InvalidValues, InvalidUtc, InvalidInterval };

[[nodiscard]] std::expected<void, WeatherInputFailure> ValidateWeatherInput(
    const WeatherPosition &at, const WeatherValues &values, int64_t selectedUtcS) noexcept;
[[nodiscard]] std::string_view WeatherInputMessage(WeatherInputFailure failure) noexcept;
}
#endif
