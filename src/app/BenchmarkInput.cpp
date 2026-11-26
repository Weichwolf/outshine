#include "BenchmarkInput.h"
#include "BenchmarkMotion.h"
#include "BenchmarkOptions.h"
#include "BenchmarkReport.h"
#include "FrameTiming.h"
#include "HostProcessor.h"
#include "ImageCapture.h"
#include "ProcessMemory.h"
#include "ScenarioInput.h"
#include "ScenarioWrite.h"
#include "format/Sha256.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <span>
#include <expected>
#include <ratio>
#include <string>
#include <string_view>

namespace outshine::App {
namespace {
constexpr Extent kDefaultBenchmarkFrame{.WidthPx = 1280, .HeightPx = 720};
}

double BenchmarkElapsedMs(BenchmarkClock::time_point since) noexcept {
  return std::chrono::duration<double, std::milli>(BenchmarkClock::now() - since).count();
}

std::expected<Scenario::Document, std::string>
ReadBenchmarkScenario(const ScenarioBenchmarkOptions &options,
                      std::string_view resourceDirectory,
                      ScenarioBenchmarkReport &report) {
  const auto input = ReadScenarioInput(options.Source, options.Overrides, resourceDirectory);
  if (!input) { return std::unexpected(input.error()); }
  const auto declaration = WriteScenario(*input);
  if (!declaration) { return std::unexpected(declaration.error()); }
  report.DeclarationSha256 = Sha256Hex(*declaration);
  report.Motion = options.Motion;
  report.Motion.RateHz = input->Render.FrameRateHz;
  if (const auto timing = FrameTiming::At(report.Motion.RateHz); !timing) {
    return std::unexpected(timing.error());
  }
  if (const auto valid = BenchmarkMotion::Make(Camera{}, report.Motion); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  report.Scenario = input->Named.Name;
  report.View = options.View.empty() ? input->Played.View : std::string(options.View);
  if (report.View.empty() && !input->Views.empty()) { report.View = input->Views.front().Id; }
  if (report.View.empty()) { return std::unexpected("scenario benchmark needs a declared view"); }
  report.Frame = input->Render.Frame == Extent{} ? kDefaultBenchmarkFrame : input->Render.Frame;
  if (report.Frame.WidthPx <= 0 || report.Frame.HeightPx <= 0) {
    return std::unexpected("benchmark frame dimensions must both be positive");
  }
  return input;
}

std::expected<void, std::string> SaveBenchmarkImage(ScenarioBenchmarkReport &report,
                                                    std::span<const uint8_t> rgba,
                                                    std::string_view path) {
  if (const auto written = SaveImage(rgba, report.Frame, path); !written) { return written; }
  const auto width = static_cast<size_t>(report.Frame.WidthPx);
  const auto height = static_cast<size_t>(report.Frame.HeightPx);
  if (width < 2) { return {}; }
  constexpr size_t channels = 4;
  constexpr size_t colourChannels = 3;
  double difference = 0;
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 1; x < width; ++x) {
      const auto at = (y * width + x) * channels;
      for (size_t channel = 0; channel < colourChannels; ++channel) {
        difference += std::abs(int{rgba[at + channel]} - int{rgba[at + channel - channels]});
      }
    }
  }
  report.ImageRowVariation =
      difference / static_cast<double>((width - 1) * height * colourChannels);
  return {};
}

void CaptureBenchmarkHost(ScenarioBenchmarkReport &report, bool before) {
  std::array<double, 3> load{};
  if (getloadavg(load.data(), static_cast<int>(load.size())) == static_cast<int>(load.size())) {
    (before ? report.LoadBefore : report.LoadAfter) = load;
  }
  if (!before) {
    report.Cpu = HostProcessorName();
    report.Memory = ReadProcessMemory();
  }
}

}
