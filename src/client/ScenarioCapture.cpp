#include "ScenarioCapture.h"
#include "FramePacer.h"
#include "CaptureCameraBasis.h"

#include <Outshine.h>
#include "io/HeapProbe.h"
#include "math/Quantile.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <limits>
#include <optional>
#include <ranges>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <cstdint>
#include <vector>

namespace outshine::Client {
namespace {

constexpr double kMaximumCaptureTimeS = 3600.0;
constexpr double kPreloadBudgetS = 30.0;
constexpr double kRefinedPreloadBudgetS = 120.0;
constexpr double kFrameBudgetMs = 1000.0 / 60.0;
constexpr double kBytesPerMiB = 1024.0 * 1024.0;
constexpr unsigned kAllRouteFields = 31u;
constexpr size_t kRouteMarkCount = 10;

struct MotionSchedule {
  size_t Ticks = 0;
  double StepS = 0.0;
};

struct MotionState {
  double StationM = 0.0;
  double Segment = 0.0;
  double EastM = 0.0;
  double UpM = 0.0;
  double RendererZM = 0.0;
  double CandidateStarts = 0.0;
  double CandidateProgress = -1.0;
  double PreviousFenceWaitMs = 0.0;
  double PreviousUploadAttempts = 0.0;
  double PreviousCrossings = 0.0;
};

struct MotionFrame {
  double TimeS = 0.0;
  MotionState State;
  CaptureCameraBasis CameraBasis;
  Vec3 CameraPositionM;
  double CameraSerial = 0.0;
  double AdvanceMs = 0.0;
  double RenderMs = 0.0;
  bool Settled = false;
  std::array<bool, 3> Contact{};
  double EyeClearanceM = std::numeric_limits<double>::quiet_NaN();
};

struct SubmittedCamera {
  CaptureCameraBasis Basis;
  Vec3 PositionM;
  double Serial = 0.0;
};

[[nodiscard]] std::optional<SubmittedCamera> ReadSubmittedCamera(const Engine &engine) {
  constexpr std::array<std::string_view, 9> names{"render last submitted camera: eye east",
                                                  "render last submitted camera: eye up",
                                                  "render last submitted camera: eye south",
                                                  "render last submitted camera: forward east",
                                                  "render last submitted camera: forward up",
                                                  "render last submitted camera: forward south",
                                                  "render last submitted camera: up east",
                                                  "render last submitted camera: up up",
                                                  "render last submitted camera: up south"};
  std::array<double, names.size()> fields{};
  unsigned found = 0;
  double serial = 0.0;
  for (const auto &measure : engine.measures()) {
    if (measure.Name == "render last submitted camera: serial") { serial = measure.Value; }
    for (size_t field = 0; field < names.size(); ++field) {
      if (measure.Name == names[field]) {
        fields[field] = measure.Value;
        found |= 1u << field;
      }
    }
  }
  constexpr unsigned allFields = (1u << names.size()) - 1u;
  if (found != allFields || !std::isfinite(serial) || serial <= 0.0 ||
      !std::ranges::all_of(fields, [](double value) { return std::isfinite(value); })) {
    return std::nullopt;
  }
  const Vec3 forward{{fields[3], fields[4], fields[5]}};
  const Vec3 up{{fields[6], fields[7], fields[8]}};
  constexpr double basisTolerance = 1e-6;
  if (std::abs(Dot(forward, forward) - 1.0) > basisTolerance ||
      std::abs(Dot(up, up) - 1.0) > basisTolerance || std::abs(Dot(forward, up)) > basisTolerance) {
    return std::nullopt;
  }
  Camera camera;
  camera.PositionM = {{fields[0], fields[1], fields[2]}};
  camera.LooksAt = true;
  camera.LookAtM = camera.PositionM + forward;
  camera.UpM = up;
  const auto basis = CaptureCameraBasis::Of(camera);
  if (!basis) { return std::nullopt; }
  return SubmittedCamera{.Basis = *basis, .PositionM = camera.PositionM, .Serial = serial};
}

struct MotionContact {
  std::array<bool, 3> Tracks{};
  double EyeClearanceM = std::numeric_limits<double>::quiet_NaN();
};

struct CaptureRoute {
  std::string Id;
  double LengthM = 0.0;
};

[[nodiscard]] std::string Refusal(std::string_view phase, std::string_view why) {
  return std::string(phase) + ": " + std::string(why);
}

[[nodiscard]] std::optional<MotionState> ReadMotionState(const Engine &engine) {
  MotionState state;
  unsigned found = 0;
  for (const DiagnosticSample &measure : engine.measures()) {
    if (measure.Name == "the route camera's station") {
      state.StationM = measure.Value;
      found |= 1u;
    } else if (measure.Name == "the route camera's segment") {
      state.Segment = measure.Value;
      found |= 2u;
    } else if (measure.Name == "the route camera's eye, east") {
      state.EastM = measure.Value;
      found |= 4u;
    } else if (measure.Name == "the route camera's eye, up") {
      state.UpM = measure.Value;
      found |= 8u;
    } else if (measure.Name == "the route camera's eye, south") {
      state.RendererZM = measure.Value;
      found |= 16u;
    } else if (measure.Name == "ground candidate: starts") {
      state.CandidateStarts = measure.Value;
    } else if (measure.Name == "ground candidate: progress") {
      state.CandidateProgress = measure.Value;
    } else if (measure.Name == "render host last submitted: fence wait") {
      state.PreviousFenceWaitMs = measure.Value;
    } else if (measure.Name == "subject residency upload attempts in all") {
      state.PreviousUploadAttempts = measure.Value;
    } else if (measure.Name == "staged crossings recorded in copy passes") {
      state.PreviousCrossings = measure.Value;
    }
  }
  if (found != kAllRouteFields || !std::isfinite(state.StationM) || !std::isfinite(state.Segment) ||
      !std::isfinite(state.EastM) || !std::isfinite(state.UpM) ||
      !std::isfinite(state.RendererZM)) {
    return std::nullopt;
  }
  return state;
}

[[nodiscard]] std::expected<std::optional<CaptureRoute>, std::string>
DeclaredCaptureRoute(const Engine &engine, std::string_view viewId) {
  for (const Scenario::View &view : engine.declaration().Views) {
    if (view.Id != viewId || view.Placement != Scenario::CameraPlacement::Route) { continue; }
    const auto info = engine.routeInfo(view.Route.RouteId);
    if (!info || !std::isfinite(info->LengthM) || info->LengthM <= 0.0) {
      return std::unexpected(info ? "capture route has no finite length"
                                  : Refusal("capture route", info.error()));
    }
    return CaptureRoute{.Id = view.Route.RouteId, .LengthM = info->LengthM};
  }
  return std::nullopt;
}

[[nodiscard]] std::expected<std::optional<CaptureRoute>, std::string>
PrepareCapturePath(Engine &engine, std::string_view viewId, double durationS) {
  auto route = DeclaredCaptureRoute(engine, viewId);
  if (!route) { return std::unexpected(route.error()); }
  if (const auto prepared = engine.prepareViewData(durationS, kRefinedPreloadBudgetS); !prepared) {
    return std::unexpected(Refusal("view data preparation", prepared.error()));
  }
  return route;
}

[[nodiscard]] std::expected<MotionContact, std::string>
ReadPublishedRoadContact(const Engine &engine, std::string_view routeId, const MotionState &state) {
  const auto pose = engine.sampleRoute(routeId, state.StationM);
  if (!pose) { return std::unexpected(Refusal("motion route", pose.error())); }
  MotionContact sampled;
  constexpr std::array kLateralFractions{0.45, 0.0, -0.45};
  for (size_t track = 0; track < sampled.Tracks.size(); ++track) {
    const auto contact =
        engine.sampleRouteContact(routeId, state.StationM, pose->WidthM * kLateralFractions[track]);
    sampled.Tracks[track] = contact.has_value();
    if (track == 1u && contact) { sampled.EyeClearanceM = state.UpM - contact->PositionM[1]; }
  }
  return sampled;
}

void RecordRoadContact(ScenarioCaptureResult &result, const MotionContact &contact) {
  result.MissingContactFrames +=
      std::ranges::all_of(contact.Tracks, [](bool found) { return found; }) ? 0u : 1u;
  if (std::isfinite(contact.EyeClearanceM)) {
    result.MinimumEyeClearanceM = std::min(
        result.MinimumEyeClearanceM.value_or(contact.EyeClearanceM), contact.EyeClearanceM);
    result.MaximumEyeClearanceM = std::max(
        result.MaximumEyeClearanceM.value_or(contact.EyeClearanceM), contact.EyeClearanceM);
  }
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

[[nodiscard]] std::expected<void, std::string>
WriteMotionTrace(std::string_view path, std::span<const MotionFrame> frames) {
  std::ofstream trace{std::string(path)};
  if (!trace) { return std::unexpected("motion trace cannot be opened"); }
  trace << "time_s\tstation_m\tsegment\teast_m\tup_m\trenderer_z_m\tadvance_ms\trender_"
           "ms\tsettled\tcandidate_starts\tcandidate_last_progress\tprevious_fence_wait_ms\t"
           "previous_upload_attempts_current_residency\tprevious_crossings_current_residency\t"
           "left_contact\tcenter_contact\tright_contact\teye_clearance_m\t"
           "camera_frame_serial\tcamera_eye_east_m\tcamera_eye_up_m\tcamera_eye_south_m\t"
           "camera_forward_east\tcamera_forward_up\tcamera_forward_south\t"
           "camera_up_east\tcamera_up_up\tcamera_up_south\n";
  trace << std::fixed << std::setprecision(6);
  for (const MotionFrame &frame : frames) {
    trace << frame.TimeS << '\t' << frame.State.StationM << '\t' << frame.State.Segment << '\t'
          << frame.State.EastM << '\t' << frame.State.UpM << '\t' << frame.State.RendererZM << '\t'
          << frame.AdvanceMs << '\t' << frame.RenderMs << '\t' << (frame.Settled ? 1 : 0) << '\t'
          << frame.State.CandidateStarts << '\t' << frame.State.CandidateProgress << '\t'
          << frame.State.PreviousFenceWaitMs << '\t' << frame.State.PreviousUploadAttempts << '\t'
          << frame.State.PreviousCrossings << '\t' << frame.Contact[0] << '\t' << frame.Contact[1]
          << '\t' << frame.Contact[2] << '\t' << frame.EyeClearanceM;
    trace << '\t' << frame.CameraSerial << std::setprecision(9);
    for (const double component : frame.CameraPositionM) { trace << '\t' << component; }
    for (const double component : frame.CameraBasis.Forward) { trace << '\t' << component; }
    for (const double component : frame.CameraBasis.Up) { trace << '\t' << component; }
    trace << std::setprecision(6) << '\n';
  }
  trace.close();
  if (!trace) { return std::unexpected("motion trace could not be written"); }
  return {};
}

[[nodiscard]] std::expected<void, std::string> RenderMotion(Engine &engine,
                                                            MotionSchedule schedule,
                                                            std::string_view routeId,
                                                            bool sampleImages,
                                                            const std::filesystem::path &folder,
                                                            std::string_view stem,
                                                            ScenarioCaptureResult &result) {
  std::vector<MotionFrame> frames;
  std::vector<double> frameMs;
  frames.reserve(schedule.Ticks);
  frameMs.reserve(schedule.Ticks);
  HeapProbe::ForgetPeak();
  FramePacer pacer(static_cast<std::uint64_t>(
      std::ceil(std::max(schedule.StepS, 1.0 / kFrameRateHz) * kNanosecondsPerSecond)));
  size_t nextMark = 0;
  const auto previousCamera = ReadSubmittedCamera(engine);
  double previousCameraSerial = previousCamera ? previousCamera->Serial : 0.0;
  for (size_t tick = 0; tick < schedule.Ticks; ++tick) {
    pacer.Wait();
    const auto began = std::chrono::steady_clock::now();
    if (const auto advanced = engine.advance(); !advanced) {
      return std::unexpected(Refusal("motion advance", advanced.error()));
    }
    const auto route = ReadMotionState(engine);
    if (!route) { return std::unexpected("motion advance did not publish a finite route pose"); }
    const auto advancedAt = std::chrono::steady_clock::now();
    if (const auto rendered = engine.renderer().render({}); !rendered) {
      return std::unexpected(Refusal("motion render", rendered.error()));
    }
    const auto renderedAt = std::chrono::steady_clock::now();
    const auto camera = ReadSubmittedCamera(engine);
    if (!camera || camera->Serial <= previousCameraSerial) {
      return std::unexpected("motion render did not submit a new valid camera frame");
    }
    previousCameraSerial = camera->Serial;
    const double advanceMs = std::chrono::duration<double, std::milli>(advancedAt - began).count();
    const double renderMs =
        std::chrono::duration<double, std::milli>(renderedAt - advancedAt).count();
    const bool settled = engine.settled(WorldQuality::Refined);
    const auto contact = ReadPublishedRoadContact(engine, routeId, *route);
    if (!contact) { return std::unexpected(contact.error()); }
    MotionFrame frame{.TimeS = static_cast<double>(tick + 1) * schedule.StepS,
                      .State = *route,
                      .CameraBasis = camera->Basis,
                      .CameraPositionM = camera->PositionM,
                      .CameraSerial = camera->Serial,
                      .AdvanceMs = advanceMs,
                      .RenderMs = renderMs,
                      .Settled = settled};
    frame.Contact = contact->Tracks;
    frame.EyeClearanceM = contact->EyeClearanceM;
    RecordRoadContact(result, *contact);
    frames.push_back(frame);
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
  result.RouteStationM = frames.back().State.StationM;
  result.HasRouteStation = true;
  result.TracePath = (folder / (std::string(stem) + "-motion.tsv")).string();
  return WriteMotionTrace(result.TracePath, frames);
}

[[nodiscard]] std::expected<void, std::string> SettleFinalFrame(Engine &engine, bool awaitRefined) {
  const WorldQuality required = awaitRefined ? WorldQuality::Refined : WorldQuality::Playable;
  const double budgetS = awaitRefined ? kRefinedPreloadBudgetS : kPreloadBudgetS;
  if (const auto ready = engine.preload(budgetS, required); !ready) {
    return std::unexpected(Refusal("capture world preload", ready.error()));
  }
  const int settleFrames = std::max(2, engine.renderer().settleFrames());
  for (int frame = 0; frame < settleFrames; ++frame) {
    if (const auto rendered = engine.renderer().render({}); !rendered) {
      return std::unexpected(Refusal("capture render", rendered.error()));
    }
  }
  return {};
}

[[nodiscard]] std::expected<void, std::string> RenderAtTime(Engine &engine,
                                                            MotionSchedule schedule,
                                                            bool isRoute,
                                                            bool awaitRefined,
                                                            ScenarioCaptureResult &result) {
  FramePacer pacer;
  for (size_t tick = 0; tick < schedule.Ticks; ++tick) {
    pacer.Wait();
    if (const auto advanced = engine.advance(); !advanced) {
      return std::unexpected(Refusal("route advance", advanced.error()));
    }
  }
  if (const auto settled = SettleFinalFrame(engine, awaitRefined); !settled) { return settled; }
  if (isRoute) {
    const auto route = ReadMotionState(engine);
    if (!route) { return std::unexpected("capture has no finite route pose"); }
    result.RouteStationM = route->StationM;
    result.HasRouteStation = true;
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
  const auto route = PrepareCapturePath(engine, options.View, result.SimTimeS);
  if (!route) { return std::unexpected(route.error()); }
  const bool isRoute = route->has_value();
  result.RouteLengthM = isRoute ? route->value().LengthM : 0.0;
  std::error_code failure;
  const std::filesystem::path folder = std::filesystem::path("build/shots") / options.Into;
  std::filesystem::create_directories(folder, failure);
  if (failure) { return std::unexpected(Refusal("capture directory", failure.message())); }
  if (options.RenderMotion) {
    if (!isRoute) { return std::unexpected("motion capture requires a route-bound view"); }
    const std::string stem = std::string(options.Name) + "-" + std::string(options.View);
    if (const auto motion = RenderMotion(engine,
                                         {.Ticks = ticks, .StepS = stepS},
                                         route->value().Id,
                                         options.SampleImages,
                                         folder,
                                         stem,
                                         result);
        !motion) {
      return std::unexpected(motion.error());
    }
    if (options.AwaitRefined) {
      if (const auto settled = SettleFinalFrame(engine, true); !settled) {
        return std::unexpected(settled.error());
      }
    }
  } else {
    if (const auto rendered = RenderAtTime(
            engine, {.Ticks = ticks, .StepS = stepS}, isRoute, options.AwaitRefined, result);
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
