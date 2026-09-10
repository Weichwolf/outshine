#ifndef OUTSHINE_SCENARIO_PLAYERVALIDATION_H
#define OUTSHINE_SCENARIO_PLAYERVALIDATION_H

#include <scenario/Scenario.h>
#include <array>
#include <cmath>
#include <expected>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view InvalidPlayer =
    "player eye height and speeds require finite nonnegative values";
}

struct PlayerField {
  const char *Name;
  double Scenario::Player::*Member;
};

inline constexpr std::array kPlayerFields{
    PlayerField{.Name = "eyeHeightM", .Member = &Scenario::Player::EyeHeightM},
    PlayerField{.Name = "walkMs", .Member = &Scenario::Player::WalkMs},
    PlayerField{.Name = "runMs", .Member = &Scenario::Player::RunMs}};

[[nodiscard]] inline std::expected<void, std::string_view>
ValidatePlayer(const Scenario::Player &player) noexcept {
  for (const auto &field : kPlayerFields) {
    const double value = player.*field.Member;
    if (!std::isfinite(value) || value < 0) { return std::unexpected(Says::InvalidPlayer); }
  }
  return {};
}
}
#endif
