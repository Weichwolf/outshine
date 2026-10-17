#ifndef OUTSHINE_GENERATORS_WEATHER_DECLAREDWEATHER_H
#define OUTSHINE_GENERATORS_WEATHER_DECLAREDWEATHER_H

#include "WeatherInput.h"
#include <world/weather/WeatherSnapshot.h>
#include <cstdint>
#include <expected>

namespace outshine::Generators::Weather {

[[nodiscard]] std::expected<WeatherSnapshot, WeatherInputFailure> MakeDeclaredWeather(
    const WeatherPosition &at, const WeatherValues &values, int64_t resolvedUtcS) noexcept;

}
#endif
