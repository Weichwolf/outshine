#ifndef OUTSHINE_CLIENT_SHOTOPTIONS_H
#define OUTSHINE_CLIENT_SHOTOPTIONS_H

#include <charconv>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Client {

inline constexpr double kDefaultPreloadSeconds = 15.0;

struct ShotOptions {
  bool Rows = false;
  bool Measures = false;
  bool Stats = false;
  bool Audit = false;
  bool Vegetation = true;
  bool All = false;
  double PreloadSeconds = kDefaultPreloadSeconds;
  size_t FirstPlace = 0;
};

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

[[nodiscard]] inline std::expected<ShotOptions, std::string_view>
ReadShotOptions(std::span<const char *const> arguments) {
  ShotOptions options;
  while (options.FirstPlace < arguments.size()) {
    const std::string_view argument = arguments[options.FirstPlace];
    if (!argument.starts_with('-')) { break; }
    ++options.FirstPlace;
    if (argument == "--rows") {
      options.Rows = true;
    } else if (argument == "--measures") {
      options.Measures = true;
    } else if (argument == "--stats") {
      options.Stats = true;
    } else if (argument == "--audit") {
      options.Audit = true;
    } else if (argument == "--no-vegetation") {
      options.Vegetation = false;
    } else if (argument == "--all") {
      if (options.FirstPlace != arguments.size()) {
        return std::unexpected("--all cannot be combined with place names or trailing options");
      }
      options.All = true;
    } else if (argument == "--preload-seconds") {
      if (options.FirstPlace == arguments.size()) {
        return std::unexpected("--preload-seconds requires a positive finite number");
      }
      const auto seconds = ReadPreloadSeconds(arguments[options.FirstPlace++]);
      if (!seconds) { return std::unexpected(seconds.error()); }
      options.PreloadSeconds = *seconds;
    } else {
      return std::unexpected("unknown shots option");
    }
  }
  return options;
}

}
#endif
