#include "EngineFactory.h"
#include "BenchmarkScenario.h"
#include "GeometrySnapshotOutput.h"
#include "BenchmarkMotion.h"
#include "BenchmarkInput.h"
#include "BenchmarkOptions.h"
#include "BenchmarkReport.h"
#include "FrameTiming.h"
#include "GpuFrameTiming.h"
#include "LogSinks.h"
#include "TextTarget.h"
#include "ApplicationRoots.h"
#include <Engine.h>
#include <SDL3/SDL.h>
#include <Scenario.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::App {
namespace {
class BenchmarkLog final : public LogSink {
public:
  void Write(double time,
             LogLevel level,
             Saying who,
             std::span<const LogField> fields) noexcept override {
    if (level >= LogLevel::Warn) { Sink_.Write(time, level, who, fields); }
  }

private:
  TextTarget Target_{TextStream::Stderr};
  TextLogSink Sink_{Target_};
};

struct VideoSystem {
  ~VideoSystem() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
};

std::expected<double, std::string> FrameSyncWait(std::span<const DiagnosticSample> measures) {
  const auto found = std::ranges::find_if(measures, [](const auto &sample) {
    return sample.Name == "render host last submitted: fence wait";
  });
  if (found == measures.end() || !std::isfinite(found->Value) || found->Value < 0) {
    return std::unexpected("the submitted frame has no valid synchronous-wait duration");
  }
  return found->Value;
}

Holds<std::vector<Vec3>>
PlanMotionCoverage(Engine &engine, double budget, ScenarioBenchmarkReport &report) {
  if (!engine.declaration().Ground.Declared || report.Motion.LinearVelocityMps == Vec3{}) {
    return std::vector<Vec3>{};
  }
  const auto began = BenchmarkClock::now();
  const auto start = engine.prepareCameraPose(budget);
  if (!start) { return std::unexpected(start.error()); }
  const auto motion = BenchmarkMotion::Make(*start, report.Motion);
  if (!motion) { return std::unexpected(std::string(motion.error())); }
  std::vector<Vec3> positions;
  positions.reserve(report.Motion.Frames + 1);
  positions.push_back(start->PositionM);
  for (size_t at = 0; at < report.Motion.Frames; ++at) {
    const auto current = motion->At(at);
    if (!current) { return std::unexpected(std::string(current.error())); }
    positions.push_back(current->PositionM);
  }
  const double remaining = budget - BenchmarkElapsedMs(began) / 1000.0;
  if (remaining <= 0) { return std::unexpected("no preload time remains to plan the camera path"); }
  const auto planned = engine.prepareCameraPathData(positions, remaining);
  if (!planned) { return std::unexpected(planned.error()); }
  report.PathPreparationMs = BenchmarkElapsedMs(began);
  return positions;
}

Result PrepareMotionData(Engine &engine,
                         std::span<const Vec3> positions,
                         const ScenarioBenchmarkOptions &options,
                         ScenarioBenchmarkReport &report) {
  if (positions.empty()) { return {}; }
  const auto began = BenchmarkClock::now();
  const double leftS =
      options.PreloadSeconds - (report.PreloadMs + report.PathPreparationMs) / 1000.0;
  if (leftS <= 0) { return std::unexpected("no preload time remains for the camera path"); }
  const auto prepared = engine.prepareCameraPathData(positions, leftS);
  report.PathPreparationMs += BenchmarkElapsedMs(began);
  return prepared;
}

Result PrepareBenchmarkWorld(Engine &engine,
                             const ScenarioBenchmarkOptions &options,
                             ScenarioBenchmarkReport &report) {
  const auto path = PlanMotionCoverage(engine, options.PreloadSeconds, report);
  if (!path) { return std::unexpected("path planning: " + path.error()); }
  const auto preloadAt = BenchmarkClock::now();
  const double preloadS = options.PreloadSeconds - report.PathPreparationMs / 1000.0;
  if (const auto ready = engine.preload(preloadS, WorldQuality::Refined); !ready) {
    return std::unexpected("preload: " + ready.error());
  }
  if (const auto focused = engine.advance(0.0); !focused) {
    return std::unexpected("initial world update: " + focused.error());
  }
  if (!engine.settled(WorldQuality::Refined)) {
    return std::unexpected("the initial benchmark view is not fully refined");
  }
  report.PreloadMs = BenchmarkElapsedMs(preloadAt);
  if (!engine.renderer().camera(report.Start)) {
    return std::unexpected("scenario benchmark has no prepared camera");
  }
  if (const auto ready = PrepareMotionData(engine, *path, options, report); !ready) {
    return std::unexpected("path preparation: " + ready.error());
  }
  report.Prepared = engine.loading();
  return {};
}

Result MeasureFrames(Engine &engine,
                     const BenchmarkMotion &motion,
                     ScenarioBenchmarkReport &report,
                     FrameTiming timing) {
  report.Frames.reserve(report.Motion.Frames);
  auto renderer = engine.renderer();
  if (const auto reset = renderer.resetMeasurements(report.Motion.Frames); !reset) { return reset; }
  const auto began = BenchmarkClock::now();
  for (size_t frame = 0; frame < report.Motion.Frames; ++frame) {
    const auto frameAt = BenchmarkClock::now();
    const auto failed = [frame](std::string_view phase, const std::string &error) {
      return std::unexpected("frame " + std::to_string(frame + 1) + " " + std::string(phase) +
                             ": " + error);
    };
    const auto pose = motion.At(frame);
    if (!pose) { return std::unexpected(std::string(pose.error())); }
    const auto applied = engine.setCameraPose(*pose);
    if (!applied) { return failed("camera", applied.error()); }
    const auto advanceAt = BenchmarkClock::now();
    const auto advanced = engine.advance(1.0 / timing.RateHz);
    if (!advanced) { return failed("update", advanced.error()); }
    const double advanceMs = BenchmarkElapsedMs(advanceAt);
    const auto renderAt = BenchmarkClock::now();
    const auto rendered = renderer.render({});
    if (!rendered) { return failed("render", rendered.error()); }
    const double renderMs = BenchmarkElapsedMs(renderAt);
    const bool refined = engine.settled(WorldQuality::Refined);
    const auto waited = FrameSyncWait(engine.measures());
    if (!waited) { return failed("wait", waited.error()); }
    const double wallMs = BenchmarkElapsedMs(frameAt);
    if (*waited > wallMs) {
      return std::unexpected("synchronous frame waiting exceeds the measured frame duration");
    }
    if (!refined && (report.Frames.empty() || report.Frames.back().Refined)) {
      report.WorldGaps.push_back(
          {.FirstFrame = frame + 1, .Blockers = engine.unsettledReasons(WorldQuality::Refined)});
    }
    report.Frames.push_back({.WallMs = wallMs,
                             .AdvanceMs = advanceMs,
                             .RenderMs = renderMs,
                             .SyncWaitMs = *waited,
                             .Refined = refined});
  }
  report.WindowMs = BenchmarkElapsedMs(began);
  if (!renderer.camera(report.Finish)) {
    return std::unexpected("the benchmark lost its final camera");
  }
  return {};
}

std::expected<std::string, std::string> ExportBenchmark(Engine &engine,
                                                        const ScenarioBenchmarkOptions &options,
                                                        const ScenarioBenchmarkReport &report) {
  auto json = ScenarioBenchmarkJson(report);
  if (!options.GeometryDirectory.empty()) {
    auto renderer = engine.renderer();
    const auto written = WriteGeometrySnapshot(renderer, options.GeometryDirectory, json);
    if (!written) { return std::unexpected(written.error()); }
  }
  return json;
}

Result FinishMeasurement(Engine &engine,
                         const ScenarioBenchmarkOptions &options,
                         ScenarioBenchmarkReport &report) {
  auto renderer = engine.renderer();
  const auto flushAt = BenchmarkClock::now();
  if (const auto flushed = renderer.flushAndWait(); !flushed) { return flushed; }
  report.FlushMs = BenchmarkElapsedMs(flushAt);
  report.GpuTiming = GpuFrameTiming::Read(engine.measures());
  if (report.GpuTiming.RenderedFrames != static_cast<double>(report.Motion.Frames)) {
    return std::unexpected("the renderer did not submit exactly the requested number of frames");
  }
  if (const auto samples = renderer.gpuSamples(report.GpuSamples); !samples) { return samples; }
  report.Finished = engine.loading();
  report.FinalWorldBlockers = engine.unsettledReasons(WorldQuality::Refined);
  const auto inspectionAt = BenchmarkClock::now();
  if (const auto inspected = engine.inspect(); !inspected) { return inspected; }
  report.InspectionMs = BenchmarkElapsedMs(inspectionAt);
  const auto measures = engine.measures();
  report.Measures.assign(measures.begin(), measures.end());
  if (!options.Image.empty()) {
    const auto imageAt = BenchmarkClock::now();
    std::vector<uint8_t> rgba;
    if (const auto read = renderer.readPixels(rgba); !read) { return read; }
    if (const auto written = SaveBenchmarkImage(report, rgba, options.Image); !written) {
      return written;
    }
    report.ImageMs = BenchmarkElapsedMs(imageAt);
  }
  return {};
}
}

std::expected<std::string, std::string>
RunScenarioBenchmark(const ScenarioBenchmarkOptions &options) {
  const auto began = BenchmarkClock::now();
  ScenarioBenchmarkReport report;
  CaptureBenchmarkHost(report, true);
  auto configuredRoots = MakeApplicationRoots(options.CacheDirectory, options.Offline);
  configuredRoots.AssetCacheDirectory = options.AssetCacheDirectory;
  auto roots = WithCacheDirectories(std::move(configuredRoots));
  if (!roots) { return std::unexpected(roots.error()); }
  auto input = ReadBenchmarkScenario(options, roots->BuiltinDataDirectory, report);
  if (!input) { return std::unexpected(input.error()); }
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    return std::unexpected("SDL video did not start: " + std::string(SDL_GetError()));
  }
  [[maybe_unused]] const VideoSystem video;
  BenchmarkLog diagnostics;
  auto engine = App::CreateEngine();
  engine.logsTo(&diagnostics);
  if (const auto rooted = engine.setRoots(std::move(*roots)); !rooted) {
    return std::unexpected(rooted.error());
  }
  if (const auto target = engine.setRenderTarget(report.Frame); !target) {
    return std::unexpected(target.error());
  }
  if (const auto declared = engine.loadScenario(*input); !declared) {
    return std::unexpected(declared.error());
  }
  if (const auto view = engine.setView(report.View); !view) {
    return std::unexpected(view.error());
  }
  report.SetupMs = BenchmarkElapsedMs(began);
  if (const auto ready = PrepareBenchmarkWorld(engine, options, report); !ready) {
    return std::unexpected(ready.error());
  }
  const auto motion = BenchmarkMotion::Make(report.Start, report.Motion);
  if (!motion) { return std::unexpected(std::string(motion.error())); }
  if (const auto measured =
          MeasureFrames(engine, *motion, report, FrameTiming{.RateHz = report.Motion.RateHz});
      !measured) {
    return std::unexpected(measured.error());
  }
  if (const auto finished = FinishMeasurement(engine, options, report); !finished) {
    return std::unexpected("measurement finalization: " + finished.error());
  }
  const auto hardwareAt = BenchmarkClock::now();
  CaptureBenchmarkHost(report, false);
  report.Gpu = engine.renderer().gpuName();
  report.GpuMemory = engine.renderer().gpuMemory();
  report.HardwareMs = BenchmarkElapsedMs(hardwareAt);
  return ExportBenchmark(engine, options, report);
}

}
