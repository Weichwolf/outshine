#ifndef OUTSHINE_SCENARIO_WORLDVALIDATION_H
#define OUTSHINE_SCENARIO_WORLDVALIDATION_H
#include <scenario/Scenario.h>
#include "GroundPollBudget.h"
#include <array>
#include <cmath>
#include <expected>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view InvalidWorld =
    "world parameters require finite coordinates, latitude in [-90,90] and nonnegative magnitudes";
}

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateWorld(const Scenario::WorldSettings &world) noexcept {
  constexpr double poleDeg = 90.0;
  if (!std::isfinite(world.Origin.LatitudeDeg) || std::abs(world.Origin.LatitudeDeg) > poleDeg ||
      !std::isfinite(world.Origin.LongitudeDeg)) {
    return std::unexpected(Says::InvalidWorld);
  }
  for (const double value :
       std::array{world.Origin.RadiusM, world.GravityMs2, world.AirDensityKgM3, world.SightM}) {
    if (!std::isfinite(value) || value < 0.0) { return std::unexpected(Says::InvalidWorld); }
  }
  if (const auto attempts = Ground::GroundPollAttempts(world.PatienceS); !attempts) {
    return std::unexpected(attempts.error());
  }
  return {};
}
}
#endif
