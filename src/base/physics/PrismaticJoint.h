#ifndef OUTSHINE_BASE_PHYSICS_PRISMATICJOINT_H
#define OUTSHINE_BASE_PHYSICS_PRISMATICJOINT_H

#include <array>
#include <cmath>
#include <expected>
#include <string_view>

namespace outshine::Physics {

struct PrismaticJoint {
  double ReachM = 0.0;
  double StiffnessNPerM = 0.0;
  double DampingNsPerM = 0.0;
  double TravelM = 0.0;
  double StopStiffnessNPerM = 0.0;
  double LoadLimitN = 0.0;
};

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
