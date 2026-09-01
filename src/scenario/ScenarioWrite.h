#ifndef OUTSHINE_SCENARIO_SCENARIOWRITE_H
#define OUTSHINE_SCENARIO_SCENARIOWRITE_H

#include <string>

#include <scenario/Scenario.h>

namespace outshine {

[[nodiscard]] std::string WriteScenario(const Scenario &declared);

}
#endif
