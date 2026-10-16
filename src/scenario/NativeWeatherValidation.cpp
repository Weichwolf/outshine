#include "NativeWeather.h"
#include "WeatherInput.h"
#include "CivilTime.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

namespace outshine {
std::expected<void, std::string_view>
ValidateScenarioWeather(const Scenario::Document &scenario,
                        std::optional<int64_t> selectedUtcS) noexcept {
  if (!scenario.Weather) { return {}; }
  if (!scenario.Time.Declared && !scenario.Time.Start.empty()) {
    return std::unexpected("native weather requires an explicitly declared clock start");
  }
  if (std::ranges::any_of(scenario.Providers, [](const Data::SourceProvider &provider) {
        return provider.Kind == "weather";
      })) {
    return std::unexpected("weather declaration and weather provider are mutually exclusive");
  }
  if (!selectedUtcS) {
    int64_t parsed = 0;
    if (ParseIsoUtc(scenario.Time.Start.c_str(), parsed)) {
      selectedUtcS = parsed;
    } else if (scenario.Time.Declared && !scenario.Time.Live) {
      return std::unexpected("declared weather requires a valid scenario clock UTC");
    }
  }
  const auto &weather = *scenario.Weather;
  const auto valid = ValidateWeatherInput(
      weather.At, weather.Values, selectedUtcS.value_or(weather.Values.ValidUtcS));
  if (!valid) { return std::unexpected(WeatherInputMessage(valid.error())); }
  return {};
}
}
