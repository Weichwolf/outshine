#ifndef OUTSHINE_SCENARIO_CAMERASPELLINGS_H
#define OUTSHINE_SCENARIO_CAMERASPELLINGS_H
#include "Spelling.h"
#include <scenario/Scenario.h>
#include <expected>
#include <string_view>

namespace outshine::CameraFormat {
namespace Says {
inline constexpr std::string_view kInvalidPlacement =
    "camera placement must be follow, local or geodetic";
}

inline constexpr Spellings<Scenario::CameraPlacement, 3> kPlacements{
    {{"follow", Scenario::CameraPlacement::FollowEntity},
     {"local", Scenario::CameraPlacement::Local},
     {"geodetic", Scenario::CameraPlacement::Geodetic}}};
static_assert(EverySpellingStandsOnce(kPlacements));

[[nodiscard]] inline std::expected<Scenario::CameraPlacement, std::string_view>
ReadPlacement(std::string_view text) noexcept {
  for (const auto &[name, mode] : kPlacements) {
    if (name == text) { return mode; }
  }
  return std::unexpected(Says::kInvalidPlacement);
}
}
#endif
