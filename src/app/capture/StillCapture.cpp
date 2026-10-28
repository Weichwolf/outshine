#include "StillCapture.h"
#include "ImageCapture.h"
#include "Quantile.h"
#include "FramePacer.h"
#include "FrameSchedule.h"
#include "ReadTextFile.h"
#include "HeapProbe.h"
#include "Sha256.h"
#include <Engine.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::App {
namespace {
constexpr int kTimedFrames = 120;
constexpr double kMillisecondsPerSecond = 1000.0;

namespace Says {
constexpr auto kTimedAdvanceFailed = " failed to advance a measured frame: ";
constexpr auto kTimedRenderFailed = " failed to render a measured frame: ";
constexpr auto kMissingTimingSamples = " has no complete frame timing sample";
}

double VariationAlongRows(std::span<const std::uint8_t> rgba, int wide, int high) {
  if (wide < 2 || high < 1 ||
      rgba.size() < static_cast<std::size_t>(wide) * static_cast<std::size_t>(high) * 4u) {
    return 0.0;
  }
  double sum = 0.0;
  std::size_t steps = 0;
  for (int y = 0; y < high; ++y) {
    for (int x = 1; x < wide; ++x) {
      const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(wide) +
                              static_cast<std::size_t>(x)) *
                             4u;
      for (int channel = 0; channel < 3; ++channel) {
        const int here = rgba[at + static_cast<std::size_t>(channel)];
        const int left = rgba[at - 4u + static_cast<std::size_t>(channel)];
        sum += here > left ? here - left : left - here;
        ++steps;
      }
    }
  }
  return steps > 0 ? sum / static_cast<double>(steps) : 0.0;
}

bool PreloadCapture(Engine &engine,
                    std::string_view name,
                    bool tells,
                    double preloadSeconds,
                    StillCaptureResult &shot) {
  const auto asked = std::chrono::steady_clock::now();
  if (tells) { std::println("    loading {} to refined world quality", name); }
  const Result ready = engine.preload(preloadSeconds, WorldQuality::Refined);
  const Loading last = engine.loading();
  const auto stood = std::chrono::steady_clock::now();
  shot.StreamedS = last.PreloadMs / kMillisecondsPerSecond;
  shot.Preloaded = ready.has_value();
  shot.LoadingMs = std::chrono::duration<double, std::milli>(stood - asked).count();
  if (!ready) {
    shot.Why = std::string(name) +
               " did not preload, so nothing measured after this point is "
               "about the declaration: " +
               ready.error();
    return false;
  }
  (void)HeapProbe::Sample();

  return true;
}

bool MeasureFrames(Engine &engine, std::string_view name, StillCaptureResult &shot) {
  if (const auto reset = engine.renderer().resetMeasurements(); !reset) {
    shot.Why = std::string(name) + " could not start frame measurements: " + reset.error();
    return false;
  }
  const auto count = static_cast<std::size_t>(kTimedFrames);
  constexpr double kBytesPerMiB = 1024 * 1024;
  std::vector<double> heldMs;
  std::vector<double> advancedMs;
  std::vector<double> renderedMs;
  heldMs.reserve(count);
  advancedMs.reserve(count);
  renderedMs.reserve(count);
  App::FramePacer pacer(shot.Timing.DeadlineNs(1));
  const auto began = std::chrono::steady_clock::now();
  const App::FrameSchedule schedule(SDL_GetTicksNS(), shot.Timing);
  for (std::size_t at = 0; at < count; ++at) {
    pacer.Wait();
    const auto before = std::chrono::steady_clock::now();
    if (const auto result = engine.advance(); !result) {
      shot.Why = std::string(name) + Says::kTimedAdvanceFailed + result.error();
      return false;
    }
    const auto advanced = std::chrono::steady_clock::now();
    if (const auto result = engine.renderer().render(Extent{}); !result) {
      shot.Why = std::string(name) + Says::kTimedRenderFailed + result.error();
      return false;
    }
    const auto rendered = std::chrono::steady_clock::now();
    (void)HeapProbe::Sample();
    advancedMs.push_back(std::chrono::duration<double, std::milli>(advanced - before).count());
    renderedMs.push_back(std::chrono::duration<double, std::milli>(rendered - advanced).count());
    heldMs.push_back(std::chrono::duration<double, std::milli>(rendered - before).count());
    shot.OverBudget += heldMs.back() > shot.Timing.BudgetMs() ? 1u : 0u;
  }
  schedule.Wait(count);
  shot.WorstAt = static_cast<std::size_t>(std::ranges::max_element(heldMs) - heldMs.begin());
  if (const auto result = engine.renderer().flushAndWait(); !result) {
    shot.Why = std::string(name) + " could not complete its measured frames: " + result.error();
    return false;
  }
  shot.MeasurementMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  shot.TurnDegrees = 0.0;
  shot.Frames = heldMs.size();
  shot.PeakHeapMB = static_cast<double>(HeapProbe::PeakLiveBytes()) / kBytesPerMiB;
  shot.PeakCostMs = HeapProbe::SampleCostMs();
  std::ranges::sort(heldMs);
  const auto p50 = QuantileOf(heldMs, kMiddleQuantile);
  const auto p95 = QuantileOf(heldMs, kBroadQuantile);
  const auto p99 = QuantileOf(heldMs, kWidestQuantile);
  const auto widest =
      [](std::vector<double> &of) -> std::expected<std::pair<double, double>, QuantileError> {
    std::ranges::sort(of);
    const auto rank = QuantileOf(of, kWidestQuantile);
    if (!rank) { return std::unexpected(rank.error()); }
    return std::pair{*rank, of.back()};
  };
  const auto advanced = widest(advancedMs);
  const auto rendered = widest(renderedMs);
  if (!p50 || !p95 || !p99 || !advanced || !rendered) {
    shot.Why = std::string(name) + Says::kMissingTimingSamples;
    return false;
  }
  shot.P50Ms = *p50;
  shot.P95Ms = *p95;
  shot.P99Ms = *p99;
  std::tie(shot.AdvanceP99Ms, shot.AdvanceWorstMs) = *advanced;
  std::tie(shot.RenderP99Ms, shot.RenderWorstMs) = *rendered;

  return true;
}

Result SaveCaptureImage(std::span<const uint8_t> pixels,
                        Extent frame,
                        std::string_view name,
                        StillCaptureResult &shot,
                        std::string_view outputDirectory) {
  const auto directory = std::filesystem::path("build/shots") / outputDirectory;
  std::error_code failed;
  std::filesystem::create_directories(directory, failed);
  if (failed) { return std::unexpected("capture output directory: " + failed.message()); }
  const auto writing = (directory / (std::string(name) + ".writing")).string();
  if (const auto saved = App::SaveImage(pixels, frame, writing); !saved) { return saved; }
  const auto bytes = ReadTextFile(writing, std::numeric_limits<size_t>::max());
  if (!bytes) {
    std::filesystem::remove(writing, failed);
    return std::unexpected(bytes.error());
  }
  shot.Digest = Sha256Hex(*bytes).substr(0, 8);
  shot.Wrote = (directory / (std::string(name) + "-" + shot.Digest + ".png")).string();
  std::filesystem::rename(writing, shot.Wrote, failed);
  if (failed) {
    const auto message = "capture output rename: " + failed.message();
    std::filesystem::remove(writing, failed);
    return std::unexpected(message);
  }
  shot.Kept = true;
  return {};
}

}

StillCaptureResult CaptureStill(Engine &engine,
                                std::string_view name,
                                bool tells,
                                std::string_view outputDirectory,
                                double preloadSeconds) {
  StillCaptureResult shot;
  const auto timing = App::FrameTiming::At(engine.declaration().Render.FrameRateHz);
  if (!timing) {
    shot.Why = timing.error();
    return shot;
  }
  shot.Timing = *timing;
  HeapProbe::ForgetPeak();
  if (!PreloadCapture(engine, name, tells, preloadSeconds, shot)) { return shot; }
  if (!MeasureFrames(engine, name, shot)) { return shot; }
  if (engine.declaration().Ground.Declared && !engine.settled(WorldQuality::Refined)) {
    shot.Why = std::string(name) + " did not reach refined world quality after " +
               std::to_string(shot.Frames) +
               " measured frames: " + engine.unsettledReasons(WorldQuality::Refined);
    return shot;
  }
  {
    auto capture = engine.beginCapture();
    if (!capture) {
      shot.Why = std::string(name) + " did not begin capture: " + capture.error();
      return shot;
    }

    const auto measured = [&engine](const char *what) {
      for (const DiagnosticSample &held : engine.measures()) {
        if (held.Name == what) { return held.Value; }
      }
      return 0.0;
    };
    shot.Triangles = measured("building triangles the world meshed");
    shot.BareTiles = measured("tiles the last rebuild laid bare");

    shot.PosedAtS = measured("and the instant it is posed at");

    std::vector<std::uint8_t> pixels;
    if (const auto read = engine.renderer().readPixels(pixels); !read) {
      shot.Why = read.error();
      return shot;
    }
    const Extent frame = engine.frameExtent();
    shot.VariationAlongRows = VariationAlongRows(pixels, frame.WidthPx, frame.HeightPx);
    if (const auto saved = SaveCaptureImage(pixels, frame, name, shot, outputDirectory); !saved) {
      shot.Why = saved.error();
      return shot;
    }
  }

  shot.Measures.assign(engine.measures().begin(), engine.measures().end());

  return shot;
}

}
