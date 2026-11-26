#include "BenchmarkCommand.h"
#include "BenchmarkOptions.h"
#include "BenchmarkScenario.h"
#include "MetalProductBenchmark.h"
#include "FarPanorama.h"
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

namespace outshine::App {
namespace {

bool ProductStoreAliases(const std::string &store, std::string_view output) {
  std::error_code error;
  const auto target = std::filesystem::weakly_canonical(output, error);
  if (error) { return false; }
  for (const auto &file : {store, store + "-wal", store + "-shm"}) {
    if (std::filesystem::equivalent(file, target, error) ||
        std::filesystem::weakly_canonical(file, error) == target) {
      return true;
    }
  }
  return false;
}

bool ProductInputAliases(const ScenarioBenchmarkOptions &scenario, std::string_view output) {
  std::error_code error;
  if (!scenario.ProductStore.empty() &&
      ProductStoreAliases(std::string(scenario.ProductStore), output)) {
    return true;
  }
  if (!scenario.TileProduct.empty() &&
      std::filesystem::equivalent(scenario.TileProduct, output, error)) {
    return true;
  }
  for (const auto panorama : {scenario.PanoramaProduct, scenario.NextPanorama}) {
    if (panorama.empty()) { continue; }
    const std::filesystem::path root(panorama);
    for (const auto name :
         {FarPanorama::MetadataFile, FarPanorama::ColourFile, FarPanorama::DepthFile}) {
      if (std::filesystem::equivalent(root / name, output, error)) { return true; }
    }
  }
  return false;
}

std::expected<void, std::string> CheckOutputPaths(const BenchmarkOptions &options) {
  if (!options.Scenario) { return {}; }
  for (const auto output : {options.Output, options.Scenario->Image}) {
    if (!output.empty() && ProductInputAliases(*options.Scenario, output)) {
      return std::unexpected("product input and benchmark output require distinct files");
    }
  }
  if (options.Output.empty() || options.Scenario->Image.empty()) { return {}; }
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
  if (options.Scenario) {
    return options.Scenario->TileProduct.empty() && options.Scenario->PanoramaProduct.empty() &&
                   options.Scenario->ProductStore.empty()
               ? RunScenarioBenchmark(*options.Scenario)
               : RunMetalProductBenchmark(*options.Scenario);
  }
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
