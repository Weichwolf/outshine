#ifndef OUTSHINE_CLIENT_SCENARIOROUNDTRIP_H
#define OUTSHINE_CLIENT_SCENARIOROUNDTRIP_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <scenario/Scenario.h>

namespace outshine::Shots {
struct Place;
}

namespace outshine::Client {
[[nodiscard]] std::expected<size_t, std::string>
RoundTripScenario(const Scenario::Document &declaration, std::string_view scratchPath);
[[nodiscard]] int RoundTripPlaces(std::span<const Shots::Place> places,
                                  std::string_view scratchPath);
}
#endif
