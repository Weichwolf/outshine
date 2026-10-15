#ifndef OUTSHINE_GENERATORS_WEATHER_OPENMETEO_H
#define OUTSHINE_GENERATORS_WEATHER_OPENMETEO_H

#include "Forecast.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Generators::Weather {

inline constexpr size_t kOpenMeteoMaximumBytes = size_t{1024} * 1024;
inline constexpr size_t kOpenMeteoMaximumHours = 4096;
inline constexpr int64_t kWeatherHourS = 3600;

enum class ForecastFailure {
  InputTooLarge,
  InvalidSource,
  HashMismatch,
  InvalidJson,
  ProviderError,
  InvalidStructure,
  InvalidUnits,
  InvalidCount,
  InvalidTime,
  InvalidValue
};

struct ForecastError {
  ForecastFailure Reason;
  const char *Field = "";
  size_t Index = 0;
};

[[nodiscard]] std::expected<Forecast, ForecastError> DecodeOpenMeteo(std::span<const uint8_t> bytes,
                                                                     const ForecastSource &source);

}
#endif
