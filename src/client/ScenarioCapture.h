#ifndef OUTSHINE_CLIENT_SCENARIOCAPTURE_H
#define OUTSHINE_CLIENT_SCENARIOCAPTURE_H

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace outshine {
class Engine;
}

namespace outshine::Client {

struct ScenarioCaptureOptions {
  std::string_view View;
  std::string_view Name;
  std::string_view Into;
  double AtS = 0.0;
  bool RenderMotion = false;
};

struct ScenarioCaptureResult {
  std::string Path;
  double SimTimeS = 0.0;
  double RouteStationM = 0.0;
  double RouteLengthM = 0.0;
  bool HasRouteStation = false;
  std::string TracePath;
  std::string LastUnsettledReason;
  size_t Frames = 0;
  size_t OverBudget = 0;
  size_t Unsettled = 0;
  double P50Ms = 0.0;
  double P95Ms = 0.0;
  double P99Ms = 0.0;
  double PeakHeapMiB = 0.0;
};

[[nodiscard]] std::expected<ScenarioCaptureResult, std::string>
CaptureScenarioView(Engine &engine, const ScenarioCaptureOptions &options);

}

#endif
