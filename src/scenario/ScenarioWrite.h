#ifndef OUTSHINE_SCENARIO_SCENARIOWRITE_H
#define OUTSHINE_SCENARIO_SCENARIOWRITE_H

#include <string>
#include <expected>

#include <scenario/Scenario.h>

namespace outshine {

[[nodiscard]] std::expected<std::string, std::string>
WriteScenario(const Scenario::Document &declared);

}
#endif
