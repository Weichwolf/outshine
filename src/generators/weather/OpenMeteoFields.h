#ifndef OUTSHINE_GENERATORS_WEATHER_OPENMETEOFIELDS_H
#define OUTSHINE_GENERATORS_WEATHER_OPENMETEOFIELDS_H

#include "OpenMeteo.h"
#include "format/Json.h"

#include <expected>
#include <optional>

namespace outshine::Generators::Weather {

[[nodiscard]] std::expected<void, ForecastError>
DecodeWeatherColumns(Json::Ref hourly, Json::Ref units, Forecast &forecast);

[[nodiscard]] std::expected<std::optional<double>, ForecastError>
OptionalWeatherNumber(Json::Ref value, const char *field, double minimum, double maximum);

[[nodiscard]] std::expected<int64_t, ForecastError> WeatherInteger(Json::Ref value,
                                                                   const char *field);

}
#endif
