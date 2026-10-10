#include "JunctionSlopePlan.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {
namespace {

constexpr double kSlopeFitToleranceM = 1e-10;
constexpr size_t kProjectionSweepsMost = 10'000;

double RiseAt(const JunctionSlopeConstraint &constraint, std::span<const JunctionSlope> slopes) {
  double riseM = 0.0;
  for (const auto &term : constraint.Terms) {
    const auto &slope = slopes[term.Junction];
    riseM += term.EastM * slope.East + term.NorthM * slope.North;
  }
  return riseM;
}

bool Project(const JunctionSlopeConstraint &constraint, std::span<JunctionSlope> slopes) {
  const double missingM = constraint.MinimumRiseM - RiseAt(constraint, slopes);
  if (missingM <= 0) { return true; }
  double lengthSquared = 0.0;
  for (const auto &term : constraint.Terms) {
    lengthSquared += term.EastM * term.EastM + term.NorthM * term.NorthM;
  }
  if (!(lengthSquared > 0)) { return false; }
  const double change = missingM / lengthSquared;
  for (const auto &term : constraint.Terms) {
    auto &slope = slopes[term.Junction];
    slope.East += change * term.EastM;
    slope.North += change * term.NorthM;
  }
  return true;
}

void LimitSlope(JunctionSlope &slope) {
  const double length = std::hypot(slope.East, slope.North);
  if (length > slope.Maximum) {
    slope.East *= slope.Maximum / length;
    slope.North *= slope.Maximum / length;
  }
}

}

std::expected<void, std::string_view>
FitJunctionSlopes(std::span<JunctionSlope> slopes,
                  std::span<const JunctionSlopeConstraint> constraints,
                  std::chrono::steady_clock::time_point deadline) {
  std::vector<uint32_t> active;
  for (const auto &constraint : constraints) {
    for (const auto &term : constraint.Terms) { active.push_back(term.Junction); }
  }
  std::ranges::sort(active);
  const auto duplicates = std::ranges::unique(active);
  active.erase(duplicates.begin(), duplicates.end());
  for (size_t sweep = 0; sweep < kProjectionSweepsMost; ++sweep) {
    if (sweep % 64 == 0 && std::chrono::steady_clock::now() >= deadline) {
      return std::unexpected("junction slope planning exceeded its deadline");
    }
    for (const auto &constraint : constraints) {
      if (!Project(constraint, slopes)) {
        return std::unexpected("road profiles need more room than junction slopes can provide");
      }
    }
    for (const auto at : active) { LimitSlope(slopes[at]); }
    const bool fitted = std::ranges::all_of(constraints, [&](const auto &constraint) {
      return RiseAt(constraint, slopes) >= constraint.MinimumRiseM - kSlopeFitToleranceM;
    });
    if (fitted) { return {}; }
  }
  return std::unexpected("junction slope planning exceeded its projection bound");
}

}
