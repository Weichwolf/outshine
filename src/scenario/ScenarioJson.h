#ifndef OUTSHINE_SCENARIO_SCENARIOJSON_H
#define OUTSHINE_SCENARIO_SCENARIOJSON_H

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace outshine {

inline constexpr size_t kMostScenarioBytes = 16u << 20u;

[[nodiscard]] inline bool IsJsonScenario(std::string_view input) noexcept {
  const auto first = input.find_first_not_of(" \t\r\n");
  return first != std::string_view::npos && (input[first] == '{' || input[first] == '[');
}

[[nodiscard]] std::expected<std::string, std::string>
ScenarioXmlFromJson(std::string_view input, std::string_view baseline = {});

}
#endif
