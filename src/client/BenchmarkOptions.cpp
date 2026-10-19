#include "BenchmarkCommand.h"
#include "BenchmarkMotion.h"
#include "BenchmarkOptions.h"
#include "ShotOptions.h"
#include "format/Number.h"
#include "math/Vec3.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace outshine::Client {
namespace {
enum class Argument : uint8_t {
  Output,
  Validation,
  Scenario,
  Overrides,
  View,
  Frames,
  Linear,
  Angular,
  Preload,
  Offline,
  Cache,
  Image
};

struct Flag {
  std::string_view Name;
  Argument Kind;
};

constexpr std::array kFlags{Flag{"--output", Argument::Output},
                            Flag{"--gpu-validation", Argument::Validation},
                            Flag{"--scenario", Argument::Scenario},
                            Flag{"--scenario-overrides", Argument::Overrides},
                            Flag{"--view", Argument::View},
                            Flag{"--frames", Argument::Frames},
                            Flag{"--linear-velocity", Argument::Linear},
                            Flag{"--angular-velocity", Argument::Angular},
                            Flag{"--preload-seconds", Argument::Preload},
                            Flag{"--offline", Argument::Offline},
                            Flag{"--cache-dir", Argument::Cache},
                            Flag{"--image", Argument::Image}};

std::expected<Vec3, std::string_view> ReadVelocity(std::string_view value) noexcept {
  Vec3 velocity{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const auto comma = value.find(',');
    if ((axis < 2) != (comma != std::string_view::npos)) {
      return std::unexpected("velocity requires exactly three comma-separated finite numbers");
    }
    const auto parsed = ParseFiniteNumber(value.substr(0, comma));
    if (!parsed) { return std::unexpected("velocity requires three finite numbers"); }
    velocity[axis] = *parsed;
    if (axis < 2) { value.remove_prefix(comma + 1); }
  }
  return velocity;
}

std::expected<void, std::string_view>
ApplyValue(BenchmarkOptions &options, Argument kind, std::string_view value) noexcept {
  if (kind == Argument::Output) {
    options.Output = value;
    return {};
  }
  if (!options.Scenario) { options.Scenario.emplace(); }
  auto &scenario = *options.Scenario;
  switch (kind) {
    case Argument::Scenario: scenario.Source = value; break;
    case Argument::View: scenario.View = value; break;
    case Argument::Image: scenario.Image = value; break;
    case Argument::Cache: scenario.CacheDirectory = value; break;
    case Argument::Overrides: {
      const auto read = ReadScenarioOverrides(value.data(), scenario.Overrides);
      if (!read) { return read; }
      break;
    }
    case Argument::Preload: {
      const auto seconds = ReadPreloadSeconds(value);
      if (!seconds) { return std::unexpected(seconds.error()); }
      scenario.PreloadSeconds = *seconds;
      break;
    }
    case Argument::Frames: {
      auto &frames = scenario.Motion.Frames;
      const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || frames == 0 ||
          frames > kMaximumBenchmarkFrames) {
        return std::unexpected("--frames requires an integer in [1,1000000]");
      }
      break;
    }
    case Argument::Linear:
    case Argument::Angular: {
      const auto velocity = ReadVelocity(value);
      if (!velocity) { return std::unexpected(velocity.error()); }
      (kind == Argument::Linear ? scenario.Motion.LinearVelocityMps
                                : scenario.Motion.AngularVelocityDegS) = *velocity;
      break;
    }
    case Argument::Output:
    case Argument::Validation:
    case Argument::Offline: return std::unexpected("benchmark argument requires no value here");
  }
  return {};
}
}

std::expected<BenchmarkOptions, std::string_view>
ReadBenchmarkOptions(std::span<const char *const> arguments) noexcept {
  BenchmarkOptions options;
  uint32_t seen = 0;
  for (size_t at = 0; at < arguments.size(); ++at) {
    if (arguments[at] == nullptr) {
      return std::unexpected("benchmark arguments must not be null");
    }
    const auto flag = std::ranges::find(kFlags, std::string_view(arguments[at]), &Flag::Name);
    if (flag == kFlags.end()) { return std::unexpected("unrecognized benchmark argument"); }
    const uint32_t bit = 1u << std::to_underlying(flag->Kind);
    if ((seen & bit) != 0) { return std::unexpected("repeated benchmark argument"); }
    seen |= bit;
    if (flag->Kind == Argument::Validation) {
      options.GpuValidation = true;
      continue;
    }
    if (flag->Kind == Argument::Offline) {
      if (!options.Scenario) { options.Scenario.emplace(); }
      options.Scenario->Offline = true;
      continue;
    }
    ++at;
    if (at == arguments.size() || arguments[at] == nullptr ||
        std::string_view(arguments[at]).empty() ||
        std::string_view(arguments[at]).starts_with("--")) {
      return std::unexpected("benchmark option requires a nonempty value");
    }
    if (const auto applied = ApplyValue(options, flag->Kind, arguments[at]); !applied) {
      return std::unexpected(applied.error());
    }
  }
  if (options.Scenario) {
    if (options.Scenario->Source.empty() || options.Scenario->Motion.Frames == 0) {
      return std::unexpected("scenario benchmark requires --scenario and --frames");
    }
    if (options.GpuValidation) {
      return std::unexpected("--gpu-validation applies only to synthetic calibration");
    }
    if (!options.Output.empty() && options.Output == options.Scenario->Image) {
      return std::unexpected("benchmark JSON and image require distinct output paths");
    }
  }
  return options;
}

}
