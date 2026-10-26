#ifndef OUTSHINE_APP_SCENARIO_SCENARIOLOADER_H
#define OUTSHINE_APP_SCENARIO_SCENARIOLOADER_H

#include "Scenario.h"
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::App {

struct ScenarioLoadRequest {
  std::string_view Source;
  std::string_view Overrides;
  std::string_view ResourceDirectory;
};

struct LoadedScenario {
  Scenario::Document Document;
  std::vector<std::string> LayerTrace;
};

[[nodiscard]] std::expected<LoadedScenario, std::string>
LoadScenario(const ScenarioLoadRequest &request);

}
#endif
