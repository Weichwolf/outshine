#ifndef OUTSHINE_CLIENT_SCENARIOCAPTURE_H
#define OUTSHINE_CLIENT_SCENARIOCAPTURE_H

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
};

struct ScenarioCaptureResult {
  std::string Path;
  double SimTimeS = 0.0;
  double RouteStationM = 0.0;
  double RouteLengthM = 0.0;
  bool HasRouteStation = false;
};

[[nodiscard]] std::expected<ScenarioCaptureResult, std::string>
CaptureScenarioView(Engine &engine, const ScenarioCaptureOptions &options);

}

#endif
