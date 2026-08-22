#ifndef OUTSHINE_SCENARIO_SCENARIOREAD_H
#define OUTSHINE_SCENARIO_SCENARIOREAD_H

#include <cstddef>
#include <string>

#include <outshine/Scenario.h>

namespace outshine {

[[nodiscard]] bool ReadScenario(const char *text, size_t length, Scenario &into,
                                std::string &error);
[[nodiscard]] bool ReadScenarioInto(const char *text, size_t length, Scenario &into,
                                    std::string &error);

}

#endif
