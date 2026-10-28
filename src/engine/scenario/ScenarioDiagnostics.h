#ifndef OUTSHINE_ENGINE_SCENARIO_SCENARIODIAGNOSTICS_H
#define OUTSHINE_ENGINE_SCENARIO_SCENARIODIAGNOSTICS_H

#include <string>
#include <vector>

namespace outshine::Scenario {
struct Document;
}

namespace outshine {
[[nodiscard]] std::vector<std::string>
DescribeScenarioDeclarations(const Scenario::Document &scenario);
}

#endif
