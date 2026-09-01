#ifndef OUTSHINE_SCENARIO_SCENARIOLAYER_H
#define OUTSHINE_SCENARIO_SCENARIOLAYER_H

#include <string>
#include <string_view>
#include <vector>

#include <scenario/Scenario.h>

namespace outshine {

[[nodiscard]] bool LayerActive(const Scenario::Layer &layer, std::string_view active);

[[nodiscard]] bool MergeLayer(Scenario::Document &into,
                              const Scenario::Document &layer,
                              std::string_view named,
                              std::vector<std::string> &trace,
                              std::string &error);

[[nodiscard]] bool ApplyLayer(Scenario::Document &into,
                              const char *text,
                              size_t size,
                              std::string_view named,
                              std::vector<std::string> &trace,
                              std::string &error);

} // namespace outshine
#endif
