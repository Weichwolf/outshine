#ifndef OUTSHINE_CLIENT_SCENARIOARGUMENTS_H
#define OUTSHINE_CLIENT_SCENARIOARGUMENTS_H

#include <charconv>
#include <cmath>
#include <expected>
#include <string_view>
#include <system_error>

namespace outshine::Client {

inline constexpr double kDefaultPreloadSeconds = 10.0;
inline constexpr std::string_view kDefaultCacheDirectory;

[[nodiscard]] constexpr bool ValidCacheDirectory(std::string_view path) noexcept {
  return !path.empty() && !path.starts_with("--");
}

[[nodiscard]] constexpr bool ValidScenarioOverrides(std::string_view input) noexcept {
  const auto first = input.find_first_not_of(" \t\r\n");
  return first != std::string_view::npos && input[first] == '{';
}

[[nodiscard]] inline std::expected<void, std::string_view>
ReadScenarioOverrides(const char *value, std::string_view &overrides) {
  if (value == nullptr || !overrides.empty() || !ValidScenarioOverrides(value)) {
    return std::unexpected("--scenario-overrides requires one JSON object");
  }
  overrides = value;
  return {};
}

[[nodiscard]] inline std::expected<double, std::string_view>
ReadPreloadSeconds(std::string_view number) {
  double seconds = 0.0;
  const auto parsed = std::from_chars(number.data(), number.data() + number.size(), seconds);
  if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
      !std::isfinite(seconds) || seconds <= 0.0) {
    return std::unexpected("--preload-seconds requires a positive finite number");
  }
  return seconds;
}

}

#endif
