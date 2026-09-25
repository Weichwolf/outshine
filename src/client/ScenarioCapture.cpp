#include "ScenarioCapture.h"

#include <Outshine.h>
#include "io/HeapProbe.h"
#include "math/Quantile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <optional>
#include <ranges>
#include <ratio>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace outshine::Client {
namespace {

constexpr double kMaximumCaptureTimeS = 3600.0;
constexpr double kPreloadBudgetS = 30.0;
constexpr double kFrameBudgetMs = 1000.0 / 60.0;
constexpr double kBytesPerMiB = 1024.0 * 1024.0;
constexpr unsigned kAllRouteFields = 31u;
constexpr size_t kRouteMarkCount = 10;

struct MotionSchedule {
  size_t Ticks = 0;
  double StepS = 0.0;
};

struct RouteMeasurements {
  double StationM = 0.0;
  double Segment = 0.0;
  double EastM = 0.0;
  double UpM = 0.0;
  double RendererZM = 0.0;
  double CandidateStarts = 0.0;
  double CandidateProgress = -1.0;
};

struct MotionFrame {
  double TimeS = 0.0;
  RouteMeasurements Route;
  double AdvanceMs = 0.0;
  double RenderMs = 0.0;
  bool Settled = false;
};

[[nodiscard]] std::string Refusal(std::string_view phase, std::string_view why) {
  return std::string(phase) + ": " + std::string(why);
}

[[nodiscard]] std::optional<RouteMeasurements> ReadRouteMeasurements(const Engine &engine) {
  RouteMeasurements route;
  unsigned found = 0;
  for (const DiagnosticSample &measure : engine.measures()) {
    if (measure.Name == "the route camera's station") {
      route.StationM = measure.Value;
      found |= 1u;
    } else if (measure.Name == "the route camera's segment") {
      route.Segment = measure.Value;
      found |= 2u;
    } else if (measure.Name == "the route camera's eye, east") {
      route.EastM = measure.Value;
      found |= 4u;
    } else if (measure.Name == "the route camera's eye, up") {
      route.UpM = measure.Value;
      found |= 8u;
    } else if (measure.Name == "the route camera's eye, south") {
      route.RendererZM = measure.Value;
      found |= 16u;
    } else if (measure.Name == "ground candidate: starts") {
      route.CandidateStarts = measure.Value;
    } else if (measure.Name == "ground candidate: progress") {
      route.CandidateProgress = measure.Value;
    }
  }
  if (found != kAllRouteFields || !std::isfinite(route.StationM) || !std::isfinite(route.Segment) ||
      !std::isfinite(route.EastM) || !std::isfinite(route.UpM) ||
      !std::isfinite(route.RendererZM)) {
    return std::nullopt;
  }
  return route;
}

[[nodiscard]] std::expected<std::optional<double>, std::string>
CaptureRouteLength(const Engine &engine, std::string_view viewId) {
  for (const Scenario::View &view : engine.declaration().Views) {
    if (view.Id != viewId || view.Placement != Scenario::CameraPlacement::Route) { continue; }
    const auto info = engine.routeInfo(view.Route.RouteId);
    if (!info || !std::isfinite(info->LengthM) || info->LengthM <= 0.0) {
      return std::unexpected(info ? "capture route has no finite length"
                                  : Refusal("capture route", info.error()));
    }
    return info->LengthM;
  }
  return std::nullopt;
}

[[nodiscard]] std::expected<void, std::string> SaveRouteMarks(Engine &engine,
                                                              double stationM,
                                                              const std::filesystem::path &folder,
                                                              std::string_view stem,
                                                              size_t &nextMark,
                                                              ScenarioCaptureResult &result) {
  while (nextMark <= kRouteMarkCount && stationM >= result.RouteLengthM *
                                                        static_cast<double>(nextMark) /
                                                        static_cast<double>(kRouteMarkCount)) {
    const auto path = folder / (std::string(stem) + "-mark" + std::to_string(nextMark) + ".png");
    if (const auto saved = engine.renderer().saveScreenshot(path.string()); !saved) {
      return std::unexpected(Refusal("route sample", saved.error()));
    }
    ++nextMark;
    ++result.SampleImages;
  }
  return {};
}

[[nodiscard]] std::expected<void, std::string> RenderMotion(Engine &engine,
                                                            MotionSchedule schedule,
                                                            bool sampleImages,
                                                            const std::filesystem::path &folder,
                                                            std::string_view stem,
                                                            ScenarioCaptureResult &result) {
  std::vector<MotionFrame> frames;
  std::vector<double> frameMs;
  frames.reserve(schedule.Ticks);
  frameMs.reserve(schedule.Ticks);
  HeapProbe::ForgetPeak();
  const auto started = std::chrono::steady_clock::now();
  size_t nextMark = 0;
  for (size_t tick = 0; tick < schedule.Ticks; ++tick) {
    const auto began = std::chrono::steady_clock::now();
    if (const auto advanced = engine.advance(); !advanced) {
      return std::unexpected(Refusal("motion advance", advanced.error()));
    }
    const auto route = ReadRouteMeasurements(engine);
    if (!route) { return std::unexpected("motion advance did not publish a finite route pose"); }
    const auto advancedAt = std::chrono::steady_clock::now();
    if (const auto rendered = engine.renderer().render({}); !rendered) {
      return std::unexpected(Refusal("motion render", rendered.error()));
    }
    const auto renderedAt = std::chrono::steady_clock::now();
    const double advanceMs = std::chrono::duration<double, std::milli>(advancedAt - began).count();
    const double renderMs =
        std::chrono::duration<double, std::milli>(renderedAt - advancedAt).count();
    const bool settled = engine.settled(WorldQuality::Refined);
    frames.push_back({.TimeS = static_cast<double>(tick + 1) * schedule.StepS,
                      .Route = *route,
                      .AdvanceMs = advanceMs,
                      .RenderMs = renderMs,
                      .Settled = settled});
    frameMs.push_back(advanceMs + renderMs);
    result.OverBudget += frameMs.back() > kFrameBudgetMs ? 1u : 0u;
    result.Unsettled += settled ? 0u : 1u;
    if (sampleImages) {
      if (const auto saved =
              SaveRouteMarks(engine, route->StationM, folder, stem, nextMark, result);
          !saved) {
        return std::unexpected(saved.error());
      }
    }
    (void)HeapProbe::Sample();
    const auto due =
        started +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(static_cast<double>(tick + 1) * schedule.StepS));
    std::this_thread::sleep_until(due);
  }
  std::ranges::sort(frameMs);
  const auto p50 = QuantileOf(frameMs, 0.50);
  const auto p95 = QuantileOf(frameMs, 0.95);
  const auto p99 = QuantileOf(frameMs, 0.99);
  if (!p50 || !p95 || !p99) { return std::unexpected("motion timings are incomplete"); }
  result.Frames = frames.size();
  result.P50Ms = *p50;
  result.P95Ms = *p95;
  result.P99Ms = *p99;
  result.PeakHeapMiB = static_cast<double>(HeapProbe::PeakLiveBytes()) / kBytesPerMiB;
  if (!frames.back().Settled) {
    result.LastUnsettledReason = engine.unsettledReasons(WorldQuality::Refined);
  }
  result.RouteStationM = frames.back().Route.StationM;
  result.HasRouteStation = true;
  result.TracePath = (folder / (std::string(stem) + "-motion.tsv")).string();
  std::ofstream trace(result.TracePath);
  if (!trace) { return std::unexpected("motion trace cannot be opened"); }
  trace << "time_s\tstation_m\tsegment\teast_m\tup_m\trenderer_z_m\tadvance_ms\trender_"
           "ms\tsettled\tcandidate_starts\tcandidate_last_progress\n";
  trace << std::fixed << std::setprecision(6);
  for (const MotionFrame &frame : frames) {
    trace << frame.TimeS << '\t' << frame.Route.StationM << '\t' << frame.Route.Segment << '\t'
          << frame.Route.EastM << '\t' << frame.Route.UpM << '\t' << frame.Route.RendererZM << '\t'
          << frame.AdvanceMs << '\t' << frame.RenderMs << '\t' << (frame.Settled ? 1 : 0) << '\t'
          << frame.Route.CandidateStarts << '\t' << frame.Route.CandidateProgress << '\n';
  }
  trace.close();
  if (!trace) { return std::unexpected("motion trace could not be written"); }
  return {};
}

[[nodiscard]] std::expected<void, std::string>
RenderAtTime(Engine &engine, MotionSchedule schedule, bool isRoute, ScenarioCaptureResult &result) {
  for (size_t tick = 0; tick < schedule.Ticks; ++tick) {
    if (const auto advanced = engine.advance(); !advanced) {
      return std::unexpected(Refusal("route advance", advanced.error()));
    }
  }
  if (const auto ready = engine.preload(kPreloadBudgetS); !ready) {
    return std::unexpected(Refusal("capture world preload", ready.error()));
  }
  if (isRoute) {
    const auto route = ReadRouteMeasurements(engine);
    if (!route) { return std::unexpected("capture has no finite route pose"); }
    result.RouteStationM = route->StationM;
    result.HasRouteStation = true;
  }
  const int settleFrames = std::max(2, engine.renderer().settleFrames());
  for (int frame = 0; frame < settleFrames; ++frame) {
    if (const auto rendered = engine.renderer().render({}); !rendered) {
      return std::unexpected(Refusal("capture render", rendered.error()));
    }
  }
  return {};
}

}

std::expected<ScenarioCaptureResult, std::string>
CaptureScenarioView(Engine &engine, const ScenarioCaptureOptions &options) {
  if (options.View.empty() || options.Name.empty() || options.Into.empty() ||
      !std::isfinite(options.AtS) || options.AtS < 0.0 || options.AtS > kMaximumCaptureTimeS) {
    return std::unexpected("capture needs a view, output name/folder and time in [0,3600] s");
  }
  if (options.SampleImages && !options.RenderMotion) {
    return std::unexpected("route samples require motion capture");
  }
  if (const auto ready = engine.preload(kPreloadBudgetS); !ready) {
    return std::unexpected(Refusal("initial world preload", ready.error()));
  }
  if (const auto selected = engine.setView(options.View); !selected) {
    return std::unexpected(Refusal("view selection", selected.error()));
  }
  const double stepS = engine.stepSeconds();
  if (!std::isfinite(stepS) || stepS <= 0.0) {
    return std::unexpected("capture requires a finite positive simulation step in seconds");
  }
  const auto ticks = static_cast<size_t>(std::max(1.0, std::ceil(options.AtS / stepS)));
  ScenarioCaptureResult result;
  result.SimTimeS = static_cast<double>(ticks) * stepS;
  const auto routeLength = CaptureRouteLength(engine, options.View);
  if (!routeLength) { return std::unexpected(routeLength.error()); }
  const bool isRoute = routeLength->has_value();
  result.RouteLengthM = routeLength->value_or(0.0);
  std::error_code failure;
  const std::filesystem::path folder = std::filesystem::path("build/shots") / options.Into;
  std::filesystem::create_directories(folder, failure);
  if (failure) { return std::unexpected(Refusal("capture directory", failure.message())); }
  if (options.RenderMotion) {
    if (!isRoute) { return std::unexpected("motion capture requires a route-bound view"); }
    const std::string stem = std::string(options.Name) + "-" + std::string(options.View);
    if (const auto motion = RenderMotion(
            engine, {.Ticks = ticks, .StepS = stepS}, options.SampleImages, folder, stem, result);
        !motion) {
      return std::unexpected(motion.error());
    }
  } else {
    if (const auto rendered =
            RenderAtTime(engine, {.Ticks = ticks, .StepS = stepS}, isRoute, result);
        !rendered) {
      return std::unexpected(rendered.error());
    }
  }
  const std::string file = std::string(options.Name) + "-" + std::string(options.View) + "-tick" +
                           std::to_string(ticks) + ".png";
  result.Path = (folder / file).string();
  if (const auto saved = engine.renderer().saveScreenshot(result.Path); !saved) {
    return std::unexpected(Refusal("capture screenshot", saved.error()));
  }
  return result;
}

}
