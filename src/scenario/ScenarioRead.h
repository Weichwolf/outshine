#ifndef OUTSHINE_SCENARIO_SCENARIOREAD_H
#define OUTSHINE_SCENARIO_SCENARIOREAD_H

#include <cstddef>
#include <string>

#include <Scenario.h>

#include "Xml.h"

namespace outshine {

[[nodiscard]] bool ReadScenario(const char *text, size_t length, Scenario &into,
                                std::string &error);
[[nodiscard]] bool ReadScenario(const Xml &document, Scenario &into, std::string &error);
[[nodiscard]] bool ReadSectionsOnto(const Xml::Ref &root, Scenario &into, std::string &error);

}

#endif
