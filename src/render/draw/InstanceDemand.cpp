#include "InstanceDemand.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
namespace {
constexpr double kDistanceGuard = 64 * std::numeric_limits<double>::epsilon();

std::optional<BatchSphere> Valid(const BatchSphere &bounds) {
  if (bounds[3] < 0 || !std::ranges::all_of(bounds, NormalOrZero)) { return std::nullopt; }
  return bounds;
}

std::optional<double> NearDistance(const BatchSphere &bounds, const Vec3 &eye) {
  double magnitude = bounds[3] + 1;
  Vec3 difference;
  for (size_t axis = 0; axis < 3; ++axis) {
    difference[axis] = static_cast<double>(bounds[axis]) - eye[axis];
    magnitude += std::abs(static_cast<double>(bounds[axis])) + std::abs(eye[axis]);
  }
  const double distance = std::hypot(difference[0], difference[1], difference[2]);
  const double error = kDistanceGuard * (magnitude + distance);
  if (!std::isfinite(distance) || !std::isfinite(error)) { return std::nullopt; }
  return std::max(0.0, distance - bounds[3] - error);
}

bool Intersects(std::span<const BatchFrustum> domains, const std::optional<BatchSphere> &bounds) {
  return std::ranges::any_of(domains,
                             [&](const BatchFrustum &domain) { return !domain.Outside(bounds); });
}

InstanceDetail
Detail(const std::optional<BatchSphere> &bounds, double errorM, const InstanceDemand &demand) {
  if (!bounds || !std::isfinite(errorM) || errorM < 0) { return InstanceDetail::Unknown; }
  const auto distance = NearDistance(*bounds, demand.EyeM);
  if (!distance) { return InstanceDetail::Unknown; }
  return demand.Projection.Allows(errorM, *distance) ? InstanceDetail::Unresolved
                                                     : InstanceDetail::Resolved;
}
}

std::expected<std::vector<InstancePatchNeed>, InstanceDemandError>
PlanInstancePatches(std::span<const BatchSphere> bounds,
                    std::span<const double> errorsM,
                    const InstanceDemand &demand) {
  if (bounds.size() != errorsM.size() || bounds.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(InstanceDemandError::InvalidCount);
  }
  if (!std::ranges::all_of(demand.EyeM, [](double value) { return std::isfinite(value); }) ||
      !std::isfinite(demand.Projection.FocalPx) || !(demand.Projection.FocalPx > 0) ||
      !std::isfinite(demand.Projection.AllowedErrorPx) || demand.Projection.AllowedErrorPx < 0) {
    return std::unexpected(InstanceDemandError::InvalidView);
  }
  std::vector<InstancePatchNeed> result;
  result.reserve(bounds.size());
  for (size_t index = 0; index < bounds.size(); ++index) {
    const auto bound = Valid(bounds[index]);
    result.push_back({.Index = static_cast<uint32_t>(index),
                      .Detail = Detail(bound, errorsM[index], demand),
                      .Visible = Intersects(demand.Views, bound),
                      .Caster = Intersects(demand.Casters, bound)});
  }
  return result;
}
}
