#include "StructureMassing.h"
#include "PlanHierarchy.h"
#include "StructureBake.h"
#include "math/Box.h"
#include "math/Vec3.h"
#include "math/Units.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr double kExplicitBlocksPerTile = 8.0;

StructureMassPlan Combined(std::span<const StructureMassPlan> plans,
                           std::span<const uint32_t> members) {
  StructureMassPlan result = plans[members.front()];
  for (size_t at = 1; at < members.size(); ++at) {
    const auto &plan = plans[members[at]];
    result.LowLat = std::min(result.LowLat, plan.LowLat);
    result.HighLat = std::max(result.HighLat, plan.HighLat);
    result.LowLon = std::min(result.LowLon, plan.LowLon);
    result.HighLon = std::max(result.HighLon, plan.HighLon);
    result.MinimumBaseM = std::min(result.MinimumBaseM, plan.MinimumBaseM);
    result.MaximumTopM = std::max(result.MaximumTopM, plan.MaximumTopM);
    result.BaseSum += plan.BaseSum;
    result.SeatSum += plan.SeatSum;
    result.HeightSum += plan.HeightSum;
    result.Count += plan.Count;
    result.PitchedAreaM2 += plan.PitchedAreaM2;
    result.RoofAreaM2 += plan.RoofAreaM2;
    for (size_t channel = 0; channel < 3; ++channel) {
      result.WallColourSum[channel] += plan.WallColourSum[channel];
    }
    result.HasWallColour = result.HasWallColour || plan.HasWallColour;
    result.Level = std::max(result.Level, plan.Level);
  }
  return result;
}

bool Accepted(const Box &bounds, const RawTile &raw) {
  const Vec3 span = bounds.Span();
  if (raw.RequestedDetail) {
    return std::max(span[0], span[2]) <= raw.TileSpanM / kExplicitBlocksPerTile;
  }
  const double away = std::max(std::hypot(std::clamp(0.0, bounds.Min[0], bounds.Max[0]),
                                          std::clamp(0.0, bounds.Min[2], bounds.Max[2])) -
                                   kStructureEyeDetailGuardM,
                               1.0);
  return raw.Projection.Allows(std::hypot(std::hypot(span[0], span[2]), span[1]), away);
}
}

std::expected<std::vector<StructureMassPlan>, StructureBakeError> GroupStructureMasses(
    std::span<StructureMassPlan> plans, const RawTile &raw, const std::atomic_bool *stopping) {
  std::ranges::sort(plans,
                    [](const auto &left, const auto &right) { return left.Cell < right.Cell; });
  std::vector<StructureMassPlan> grouped;
  grouped.reserve(plans.size());
  for (size_t first = 0; first < plans.size();) {
    if (stopping != nullptr && stopping->load(std::memory_order_relaxed)) {
      return std::unexpected(StructureBakeErrorKind::Cancelled);
    }
    size_t end = first + 1;
    while (end < plans.size() && plans[end].Cell == plans[first].Cell) { ++end; }
    const auto cellPlans = plans.subspan(first, end - first);
    const double referenceLat = raw.RequestedDetail
                                    ? 0.5 * (cellPlans.front().LowLat + cellPlans.front().HighLat)
                                    : raw.Eye.LatitudeDeg;
    const double referenceLon = raw.RequestedDetail
                                    ? 0.5 * (cellPlans.front().LowLon + cellPlans.front().HighLon)
                                    : raw.Eye.LongitudeDeg;
    const double lonScale = kMPerDegLon * std::cos(referenceLat * kDeg2Rad);
    std::vector<Box> bounds;
    bounds.reserve(cellPlans.size());
    for (const auto &plan : cellPlans) {
      const double centreLon = 0.5 * (plan.LowLon + plan.HighLon);
      const double eyeLon = centreLon + std::remainder(referenceLon - centreLon, kDegPerTurn);
      bounds.push_back({.Min = {{(plan.LowLon - eyeLon) * lonScale,
                                 plan.MinimumBaseM,
                                 (plan.LowLat - referenceLat) * kMPerDegLat}},
                        .Max = {{(plan.HighLon - eyeLon) * lonScale,
                                 plan.MaximumTopM,
                                 (plan.HighLat - referenceLat) * kMPerDegLat}}});
    }
    auto hierarchy = PlanHierarchy::Build(bounds);
    if (!hierarchy) { return std::unexpected(StructureBakeErrorKind::InvalidCell); }
    hierarchy->Select([&](const Box &node) { return Accepted(node, raw); },
                      [&](const PlanHierarchy::Node &node) {
                        grouped.push_back(Combined(cellPlans, hierarchy->Members(node)));
                      });
    first = end;
  }
  return grouped;
}

}
