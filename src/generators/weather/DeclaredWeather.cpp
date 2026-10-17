#include "DeclaredWeather.h"
#include "WeatherInput.h"

#include <cstdint>
#include <expected>

namespace outshine::Generators::Weather {
std::expected<WeatherSnapshot, WeatherInputFailure> MakeDeclaredWeather(
    const WeatherPosition &at, const WeatherValues &values, int64_t resolvedUtcS) noexcept {
  if (const auto valid = ValidateWeatherInput(at, values, resolvedUtcS); !valid) {
    return std::unexpected(valid.error());
  }
  WeatherSnapshot snapshot{.Values = values};
  snapshot.Source.Origin = WeatherOrigin::ScenarioDeclaration;
  snapshot.Source.Requested = at;
  return snapshot;
}
}
