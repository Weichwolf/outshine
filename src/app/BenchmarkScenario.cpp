#include "EngineFactory.h"
#include "BenchmarkScenario.h"
#include "BenchmarkMotion.h"
#include "BenchmarkOptions.h"
#include "BenchmarkReport.h"
#include "FrameTiming.h"
#include "GpuFrameTiming.h"
#include "HostProcessor.h"
#include "ImageCapture.h"
#include "ScenarioInput.h"
#include "ScenarioWrite.h"
#include "ApplicationRoots.h"
#include "format/Sha256.h"
#include <Engine.h>
#include <SDL3/SDL.h>
#include <Scenario.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::App {
namespace {
using Clock = std::chrono::steady_clock;
constexpr Extent kDefaultBenchmarkFrame{.WidthPx = 1280, .HeightPx = 720};

double ElapsedMs(Clock::time_point since) noexcept {
  return std::chrono::duration<double, std::milli>(Clock::now() - since).count();
}

std::optional<std::array<double, 3>> HostLoad() noexcept {
  std::array<double, 3> load{};
  if (getloadavg(load.data(), static_cast<int>(load.size())) != static_cast<int>(load.size())) {
    return std::nullopt;
  }
  return load;
}

struct VideoSystem {
  ~VideoSystem() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
};

Result MeasureFrames(Engine &engine,
                     const BenchmarkMotion &motion,
                     ScenarioBenchmarkReport &report,
                     FrameTiming timing) {
  report.Frames.reserve(report.Motion.Frames);
  auto renderer = engine.renderer();
  if (const auto reset = renderer.resetMeasurements(report.Motion.Frames); !reset) { return reset; }
  const auto began = Clock::now();
  for (size_t frame = 0; frame < report.Motion.Frames; ++frame) {
    const auto frameAt = Clock::now();
    const auto pose = motion.At(frame);
    if (!pose) { return std::unexpected(std::string(pose.error())); }
    const auto applied = engine.setCameraPose(*pose);
    if (!applied) { return applied; }
    const auto advanceAt = Clock::now();
    const auto advanced = engine.advance(1.0 / timing.RateHz);
    if (!advanced) { return advanced; }
    const double advanceMs = ElapsedMs(advanceAt);
    const auto renderAt = Clock::now();
    const auto rendered = renderer.render({});
    if (!rendered) { return rendered; }
    const double renderMs = ElapsedMs(renderAt);
    const bool refined = engine.settled(WorldQuality::Refined);
    report.Frames.push_back({.WallMs = ElapsedMs(frameAt),
                             .AdvanceMs = advanceMs,
                             .RenderMs = renderMs,
                             .Refined = refined});
  }
  report.WindowMs = ElapsedMs(began);
  if (!renderer.camera(report.Finish)) {
    return std::unexpected("the benchmark lost its final camera");
  }
  return {};
}

Result FinishMeasurement(Engine &engine,
                         const ScenarioBenchmarkOptions &options,
                         ScenarioBenchmarkReport &report) {
  auto renderer = engine.renderer();
  const auto flushAt = Clock::now();
  if (const auto flushed = renderer.flushAndWait(); !flushed) { return flushed; }
  report.FlushMs = ElapsedMs(flushAt);
  const auto metrics = GpuFrameTiming::Read(engine.measures());
  if (metrics.RenderedFrames != static_cast<double>(report.Motion.Frames)) {
    return std::unexpected("the renderer did not submit exactly the requested number of frames");
  }
  if (const auto samples = renderer.gpuSamples(report.GpuSamples); !samples) { return samples; }
  report.Finished = engine.loading();
  report.FinalWorldBlockers = engine.unsettledReasons(WorldQuality::Refined);
  const auto measures = engine.measures();
  report.Measures.assign(measures.begin(), measures.end());
  if (!options.Image.empty()) {
    const auto imageAt = Clock::now();
    std::vector<uint8_t> rgba;
    if (const auto read = renderer.readPixels(rgba); !read) { return read; }
    if (const auto written = App::SaveImage(rgba, report.Frame, options.Image); !written) {
      return written;
    }
    report.ImageMs = ElapsedMs(imageAt);
  }
  return {};
}
}

std::expected<std::string, std::string>
RunScenarioBenchmark(const ScenarioBenchmarkOptions &options) {
  const auto began = Clock::now();
  ScenarioBenchmarkReport report;
  report.LoadBefore = HostLoad();
  auto roots = WithCacheDirectories(MakeApplicationRoots(options.CacheDirectory, options.Offline));
  if (!roots) { return std::unexpected(roots.error()); }
  auto input = ReadScenarioInput(options.Source, options.Overrides, roots->Shipped);
  if (!input) { return std::unexpected(input.error()); }
  auto &document = *input;
  const auto declaration = WriteScenario(document);
  if (!declaration) { return std::unexpected(declaration.error()); }
  report.DeclarationSha256 = Sha256Hex(*declaration);
  report.Motion = options.Motion;
  report.Motion.RateHz = document.Render.FrameRateHz;
  const auto timing = FrameTiming::At(report.Motion.RateHz);
  if (!timing) { return std::unexpected(timing.error()); }
  if (const auto valid = BenchmarkMotion::Make(Camera{}, report.Motion); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  report.Scenario = document.Named.Name;
  report.View = options.View.empty() ? document.Played.View : std::string(options.View);
  if (report.View.empty() && !document.Views.empty()) { report.View = document.Views.front().Id; }
  if (report.View.empty()) { return std::unexpected("scenario benchmark needs a declared view"); }
  report.Frame = document.Render.Frame == Extent{} ? kDefaultBenchmarkFrame : document.Render.Frame;
  if (report.Frame.WidthPx <= 0 || report.Frame.HeightPx <= 0) {
    return std::unexpected("benchmark frame dimensions must both be positive");
  }
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    return std::unexpected("SDL video did not start: " + std::string(SDL_GetError()));
  }
  [[maybe_unused]] const VideoSystem video;
  auto engine = App::CreateEngine();
  if (const auto rooted = engine.setRoots(std::move(*roots)); !rooted) {
    return std::unexpected(rooted.error());
  }
  if (const auto target = engine.setRenderTarget(report.Frame); !target) {
    return std::unexpected(target.error());
  }
  if (const auto declared = engine.declare(document); !declared) {
    return std::unexpected(declared.error());
  }
  if (const auto assembled = engine.assemble(); !assembled) {
    return std::unexpected(assembled.error());
  }
  if (const auto view = engine.setView(report.View); !view) {
    return std::unexpected(view.error());
  }
  report.SetupMs = ElapsedMs(began);
  const auto preloadAt = Clock::now();
  if (const auto ready = engine.preload(options.PreloadSeconds, WorldQuality::Refined); !ready) {
    return std::unexpected(ready.error());
  }
  if (const auto focused = engine.advance(0.0); !focused) {
    return std::unexpected(focused.error());
  }
  if (!engine.settled(WorldQuality::Refined)) {
    return std::unexpected("the initial benchmark view is not fully refined");
  }
  report.PreloadMs = ElapsedMs(preloadAt);
  report.Prepared = engine.loading();
  if (!engine.renderer().camera(report.Start)) {
    return std::unexpected("scenario benchmark has no prepared camera");
  }
  const auto motion = BenchmarkMotion::Make(report.Start, report.Motion);
  if (!motion) { return std::unexpected(std::string(motion.error())); }
  if (const auto measured = MeasureFrames(engine, *motion, report, *timing); !measured) {
    return std::unexpected(measured.error());
  }
  if (const auto finished = FinishMeasurement(engine, options, report); !finished) {
    return std::unexpected(finished.error());
  }
  const auto hardwareAt = Clock::now();
  report.Cpu = HostProcessorName();
  report.Gpu = engine.renderer().gpuName();
  report.HardwareMs = ElapsedMs(hardwareAt);
  report.LoadAfter = HostLoad();
  return ScenarioBenchmarkJson(report);
}

}
