#ifndef OUTSHINE_CLIENT_SHOTOPTIONS_H
#define OUTSHINE_CLIENT_SHOTOPTIONS_H

#include "ScenarioArguments.h"
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Client {

struct ShotOptions {
  bool Rows = false;
  bool Measures = false;
  bool Stats = false;
  bool Audit = false;
  bool Offline = false;
  bool All = false;
  double PreloadSeconds = kDefaultPreloadSeconds;
  std::string_view CacheDirectory = kDefaultCacheDirectory;
  std::string_view ScenarioOverrides;
  size_t FirstPlace = 0;
};

[[nodiscard]] inline bool ReadShotFlag(std::string_view argument, ShotOptions &options) {
  if (argument == "--rows") {
    options.Rows = true;
  } else if (argument == "--measures") {
    options.Measures = true;
  } else if (argument == "--stats") {
    options.Stats = true;
  } else if (argument == "--audit") {
    options.Audit = true;
  } else if (argument == "--offline") {
    options.Offline = true;
  } else {
    return false;
  }
  return true;
}

[[nodiscard]] inline std::expected<void, std::string_view> ReadShotValue(
    std::string_view argument, std::span<const char *const> arguments, ShotOptions &options) {
  const char *value =
      options.FirstPlace == arguments.size() ? nullptr : arguments[options.FirstPlace++];
  if (argument == "--cache-dir") {
    if (value == nullptr || !ValidCacheDirectory(value)) {
      return std::unexpected("--cache-dir requires a nonempty directory");
    }
    options.CacheDirectory = value;
  } else if (argument == "--scenario-overrides") {
    return ReadScenarioOverrides(value, options.ScenarioOverrides);
  } else if (argument == "--preload-seconds") {
    if (value == nullptr) {
      return std::unexpected("--preload-seconds requires a positive finite number");
    }
    const auto seconds = ReadPreloadSeconds(value);
    if (!seconds) { return std::unexpected(seconds.error()); }
    options.PreloadSeconds = *seconds;
  } else {
    return std::unexpected("unknown shots option");
  }
  return {};
}

[[nodiscard]] inline std::expected<ShotOptions, std::string_view>
ReadShotOptions(std::span<const char *const> arguments) {
  ShotOptions options;
  while (options.FirstPlace < arguments.size()) {
    const std::string_view argument = arguments[options.FirstPlace];
    if (!argument.starts_with('-')) { break; }
    ++options.FirstPlace;
    if (ReadShotFlag(argument, options)) { continue; }
    if (argument == "--all") {
      if (options.FirstPlace != arguments.size()) {
        return std::unexpected("--all cannot be combined with place names or trailing options");
      }
      options.All = true;
    } else {
      const auto read = ReadShotValue(argument, arguments, options);
      if (!read) { return std::unexpected(read.error()); }
    }
  }
  return options;
}

}
#endif
