#include "BenchmarkCommand.h"
#include "Calibration.h"
#include "WriteFileAtomically.h"

#include <cstddef>
#include <cstdio>
#include <expected>
#include <print>
#include <span>
#include <string_view>

namespace outshine::Client {

std::expected<BenchmarkOptions, std::string_view>
ReadBenchmarkOptions(std::span<const char *const> arguments) noexcept {
  BenchmarkOptions options;
  for (size_t at = 0; at < arguments.size(); ++at) {
    if (arguments[at] == nullptr) {
      return std::unexpected("benchmark arguments must not be null");
    }
    const std::string_view argument = arguments[at];
    if (argument == "--gpu-validation" && !options.GpuValidation) {
      options.GpuValidation = true;
    } else if (argument == "--output" && options.Output.empty()) {
      ++at;
      if (at == arguments.size() || arguments[at] == nullptr ||
          std::string_view(arguments[at]).empty() ||
          std::string_view(arguments[at]).starts_with("--")) {
        return std::unexpected("--output requires a file path");
      }
      options.Output = arguments[at];
    } else {
      return std::unexpected("unrecognized or repeated benchmark argument");
    }
  }
  return options;
}

int RunBenchmarkCommand(std::span<const char *const> arguments) {
  const auto options = ReadBenchmarkOptions(arguments);
  if (!options) {
    std::println(stderr, "outshine benchmark: {}", options.error());
    return 2;
  }
  const auto report = Diagnostics::Calibration::Run(options->GpuValidation);
  if (!report) {
    std::println(stderr, "outshine benchmark: {}", report.error().Message);
    return 1;
  }
  const auto json = Diagnostics::Calibration::Json(*report);
  if (options->Output.empty()) {
    if (std::fwrite(json.data(), 1, json.size(), stdout) != json.size()) {
      std::println(stderr, "outshine benchmark: result output failed");
      return 1;
    }
    return 0;
  }
  const auto written = WriteFileAtomically(options->Output, std::as_bytes(std::span(json)));
  if (!written) {
    std::println(stderr, "outshine benchmark: {}", written.error());
    return 1;
  }
  return 0;
}

}
