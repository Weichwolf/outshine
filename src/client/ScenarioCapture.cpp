#include "ScenarioCapture.h"

#include <Outshine.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine::Client {
namespace {

constexpr double kMaximumCaptureTimeS = 3600.0;
constexpr double kPreloadBudgetS = 30.0;

[[nodiscard]] std::string Refusal(std::string_view phase, std::string_view why) {
  return std::string(phase) + ": " + std::string(why);
}

}

std::expected<ScenarioCaptureResult, std::string>
CaptureScenarioView(Engine &engine, const ScenarioCaptureOptions &options) {
  if (options.View.empty() || options.Name.empty() || options.Into.empty() ||
      !std::isfinite(options.AtS) || options.AtS < 0.0 || options.AtS > kMaximumCaptureTimeS) {
    return std::unexpected("capture needs a view, output name/folder and time in [0,3600] s");
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
  for (size_t tick = 0; tick < ticks; ++tick) {
    if (const auto advanced = engine.advance(); !advanced) {
      return std::unexpected(Refusal("route advance", advanced.error()));
    }
  }
  if (const auto ready = engine.preload(kPreloadBudgetS); !ready) {
    return std::unexpected(Refusal("capture world preload", ready.error()));
  }
  ScenarioCaptureResult result;
  result.SimTimeS = static_cast<double>(ticks) * stepS;
  for (const Scenario::View &view : engine.declaration().Views) {
    if (view.Id != options.View || view.Placement != Scenario::CameraPlacement::Route) { continue; }
    const auto info = engine.routeInfo(view.Route.RouteId);
    if (!info) { return std::unexpected(Refusal("capture route", info.error())); }
    result.RouteLengthM = info->LengthM;
    break;
  }
  for (const DiagnosticSample &measure : engine.measures()) {
    if (measure.Name == "the route camera's station") {
      result.RouteStationM = measure.Value;
      result.HasRouteStation = true;
      break;
    }
  }
  const int settleFrames = std::max(2, engine.renderer().settleFrames());
  for (int frame = 0; frame < settleFrames; ++frame) {
    if (const auto rendered = engine.renderer().render({}); !rendered) {
      return std::unexpected(Refusal("capture render", rendered.error()));
    }
  }
  std::error_code failure;
  const std::filesystem::path folder = std::filesystem::path("build/shots") / options.Into;
  std::filesystem::create_directories(folder, failure);
  if (failure) { return std::unexpected(Refusal("capture directory", failure.message())); }
  const std::string file = std::string(options.Name) + "-" + std::string(options.View) + "-tick" +
                           std::to_string(ticks) + ".png";
  result.Path = (folder / file).string();
  if (const auto saved = engine.renderer().saveScreenshot(result.Path); !saved) {
    return std::unexpected(Refusal("capture screenshot", saved.error()));
  }
  return result;
}

}
