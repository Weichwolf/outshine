#include "BenchmarkCommand.h"
#include "BenchmarkOptions.h"
#include "BenchmarkScenario.h"
#include "Calibration.h"
#include "WriteFileAtomically.h"

#include <cstddef>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine::Client {
namespace {

std::expected<void, std::string> CheckOutputPaths(const BenchmarkOptions &options) {
  if (!options.Scenario || options.Output.empty() || options.Scenario->Image.empty()) { return {}; }
  std::error_code error;
  const auto output = std::filesystem::weakly_canonical(options.Output, error);
  if (error) {
    return std::unexpected("benchmark JSON path cannot be resolved: " + error.message());
  }
  const auto image = std::filesystem::weakly_canonical(options.Scenario->Image, error);
  if (error) {
    return std::unexpected("benchmark image path cannot be resolved: " + error.message());
  }
  const bool sameFile = std::filesystem::equivalent(output, image, error);
  if (output == image || (!error && sameFile)) {
    return std::unexpected("benchmark JSON and image require distinct output paths");
  }
  return {};
}

std::expected<std::string, std::string> ExecuteBenchmark(const BenchmarkOptions &options) {
  if (options.Scenario) { return RunScenarioBenchmark(*options.Scenario); }
  const auto report = Diagnostics::Calibration::Run(options.GpuValidation);
  if (!report) { return std::unexpected(report.error().Message); }
  return Diagnostics::Calibration::Json(*report);
}
}

int RunBenchmarkCommand(std::span<const char *const> arguments) {
  const auto options = ReadBenchmarkOptions(arguments);
  if (!options) {
    std::println(stderr, "outshine benchmark: {}", options.error());
    return 2;
  }
  if (const auto paths = CheckOutputPaths(*options); !paths) {
    std::println(stderr, "outshine benchmark: {}", paths.error());
    return 2;
  }
  const auto result = ExecuteBenchmark(*options);
  if (!result) {
    std::println(stderr, "outshine benchmark: {}", result.error());
    return 1;
  }
  const auto &json = *result;
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
