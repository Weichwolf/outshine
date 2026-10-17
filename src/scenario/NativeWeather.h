#ifndef OUTSHINE_SCENARIO_NATIVEWEATHER_H
#define OUTSHINE_SCENARIO_NATIVEWEATHER_H

#include <scenario/Scenario.h>
#include "Xml.h"
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace outshine {
[[nodiscard]] bool ReadNativeWeather(const Xml::Ref &from,
                                     std::optional<Scenario::WeatherDeclaration> &into,
                                     std::string &error);
void WriteNativeWeather(std::string &into,
                        const std::optional<Scenario::WeatherDeclaration> &weather);
[[nodiscard]] std::expected<void, std::string_view>
ValidateScenarioWeather(const Scenario::Document &scenario,
                        std::optional<int64_t> selectedUtcS = std::nullopt) noexcept;
}
#endif
