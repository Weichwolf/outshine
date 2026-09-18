#ifndef OUTSHINE_PHYSICS_PRISMATICJOINT_H
#define OUTSHINE_PHYSICS_PRISMATICJOINT_H

#include <array>
#include <cmath>
#include <expected>
#include <string_view>

namespace outshine::Physics {

/// Unilateral spring-damper joint parameters in SI units.
struct PrismaticJoint {
  double ReachM = 0.0;             ///< Clearance below this reach compresses the joint.
  double StiffnessNPerM = 0.0;     ///< Spring stiffness through the permitted travel.
  double DampingNsPerM = 0.0;      ///< Load added per closing speed.
  double TravelM = 0.0;            ///< Spring travel; zero disables the travel stop.
  double StopStiffnessNPerM = 0.0; ///< Additional stiffness beyond positive travel.
  double LoadLimitN = 0.0;         ///< Overload threshold; zero disables reporting.
};

/// Validate finite, nonnegative joint parameters without mutation or allocation.
/// @return Success or a static diagnostic shared by direct and imported declarations.
[[nodiscard]] inline std::expected<void, std::string_view>
ValidatePrismaticJoint(const PrismaticJoint &joint) noexcept {
  constexpr std::string_view invalid =
      "prismatic joint requires finite nonnegative reach, stiffness, damping, travel, stop "
      "stiffness and load limit";
  for (const double value : std::array{joint.ReachM,
                                       joint.StiffnessNPerM,
                                       joint.DampingNsPerM,
                                       joint.TravelM,
                                       joint.StopStiffnessNPerM,
                                       joint.LoadLimitN}) {
    if (!std::isfinite(value) || value < 0.0) { return std::unexpected(invalid); }
  }
  return {};
}

}

#endif
