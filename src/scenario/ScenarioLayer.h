#ifndef OUTSHINE_SCENARIO_SCENARIOLAYER_H
#define OUTSHINE_SCENARIO_SCENARIOLAYER_H

#include <string>
#include <string_view>
#include <vector>

#include <Scenario.h>

namespace outshine {

[[nodiscard]] bool LayerActive(const Layer &layer, std::string_view active);

[[nodiscard]] bool MergeLayer(Scenario &into,
                              const Scenario &layer,
                              std::string_view named,
                              std::vector<std::string> &trace,
                              std::string &error);

[[nodiscard]] bool ApplyLayer(Scenario &into,
                              const char *text,
                              size_t size,
                              std::string_view named,
                              std::vector<std::string> &trace,
                              std::string &error);

} // namespace outshine
#endif
