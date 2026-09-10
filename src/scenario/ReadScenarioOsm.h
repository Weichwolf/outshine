#ifndef OUTSHINE_SCENARIO_READSCENARIOOSM_H
#define OUTSHINE_SCENARIO_READSCENARIOOSM_H
#include <expected>
#include <string_view>
#include <vector>
#include <scenario/Scenario.h>
#include "Xml.h"

namespace outshine {
[[nodiscard]] std::expected<void, std::string_view>
ReadScenarioOsm(const Xml::Ref &osm, std::vector<Scenario::Structure> &into);
}
#endif
