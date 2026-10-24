#include "FootpathConnectionInternal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

#include "Earth.h"
#include "PlanHierarchy.h"
#include "math/Box.h"

namespace outshine::Generators::FootpathConnections {
namespace {

constexpr size_t kMaximumDeclaredWork = 2000000;
constexpr size_t kMaximumSourcePoints = 4096;
constexpr double kMaximumCoordinateM = 1e7;
constexpr double kMinimumSegmentM = 1e-6;
constexpr double kMaximumHalfWidthM = 100;
constexpr double kMaximumReachM = 10;

bool ValidPoint(EastNorth point) {
  return std::isfinite(point.EastM) && std::isfinite(point.NorthM) &&
         std::abs(point.EastM) <= kMaximumCoordinateM &&
         std::abs(point.NorthM) <= kMaximumCoordinateM;
}

bool ValidSource(const FootpathSource &source) {
  return source.Axis.size() >= 2 && source.Axis.size() <= kMaximumSourcePoints &&
         std::isfinite(source.HalfWidthM) && source.HalfWidthM > 0 &&
         source.HalfWidthM <= kMaximumHalfWidthM && std::ranges::all_of(source.Axis, ValidPoint) &&
         (!source.PositionErrorBoundM ||
          (std::isfinite(*source.PositionErrorBoundM) && *source.PositionErrorBoundM >= 0 &&
           *source.PositionErrorBoundM <= kMaximumHalfWidthM));
}

Box Bounds(EastNorth a, EastNorth b) {
  return {.Min = {{std::min(a.EastM, b.EastM), std::min(a.NorthM, b.NorthM), 0}},
          .Max = {{std::max(a.EastM, b.EastM), std::max(a.NorthM, b.NorthM), 0}}};
}

}

bool GroundFoot(const FootpathSource &source) {
  return source.Layer.has_value() && source.Bridge == false && source.Tunnel == false &&
         source.FootAccess == true;
}

FootpathEndDisposition Disposition(const FootpathSource &source, size_t end) {
  if (!GroundFoot(source)) { return FootpathEndDisposition::UnknownGroundAccess; }
  switch (source.Ends[end]) {
    case FootpathEndRole::Unknown: return FootpathEndDisposition::UnknownEnd;
    case FootpathEndRole::Terminal: return FootpathEndDisposition::Terminal;
    case FootpathEndRole::Fragment: return FootpathEndDisposition::Fragment;
    case FootpathEndRole::Fixed: return FootpathEndDisposition::FixedBoundary;
    case FootpathEndRole::Connectable: return FootpathEndDisposition::NoTarget;
  }
  return FootpathEndDisposition::UnknownEnd;
}

std::expected<Inputs, FootpathConnectionError>
PrepareInputs(std::span<const FootpathSource> sources,
              FootpathConnectionRecipe recipe,
              FootpathConnectionBudget budget,
              const std::stop_token &stop) {
  if (budget.Segments > kMaximumDeclaredWork || budget.Endpoints > kMaximumDeclaredWork ||
      budget.Candidates > kMaximumDeclaredWork || !std::isfinite(recipe.ReachM) ||
      recipe.ReachM < 0 || recipe.ReachM > kMaximumReachM ||
      !std::isfinite(recipe.MinimumCrossingSine) || recipe.MinimumCrossingSine <= 0 ||
      recipe.MinimumCrossingSine > 1) {
    return std::unexpected(FootpathConnectionError::InvalidInput);
  }
  if (sources.size() > budget.Segments) {
    return std::unexpected(FootpathConnectionError::CapacityExceeded);
  }
  Inputs inputs;
  inputs.Lengths.reserve(sources.size());
  for (const auto &source : sources) {
    if (stop.stop_requested()) { return std::unexpected(FootpathConnectionError::Canceled); }
    if (!ValidSource(source)) { return std::unexpected(FootpathConnectionError::InvalidInput); }
    inputs.Segments += source.Axis.size() - 1;
    if (source.Kind == FootpathKind::Steps) { inputs.Endpoints += 2; }
    if (inputs.Segments > budget.Segments || inputs.Endpoints > budget.Endpoints) {
      return std::unexpected(FootpathConnectionError::CapacityExceeded);
    }
    double length = 0.0;
    for (size_t at = 1; at < source.Axis.size(); ++at) {
      const double segment = std::hypot(source.Axis[at].EastM - source.Axis[at - 1].EastM,
                                        source.Axis[at].NorthM - source.Axis[at - 1].NorthM);
      if (segment < kMinimumSegmentM) {
        return std::unexpected(FootpathConnectionError::InvalidInput);
      }
      length += segment;
    }
    inputs.Lengths.push_back(length);
    if (source.Kind == FootpathKind::Steps && GroundFoot(source)) {
      inputs.EligibleEndpoints += std::ranges::count(source.Ends, FootpathEndRole::Connectable);
    }
  }
  return inputs;
}

std::expected<Index, FootpathConnectionError> BuildIndex(std::span<const FootpathSource> sources,
                                                         const std::stop_token &stop) {
  Index index;
  std::vector<Box> bounds;
  for (size_t source = 0; source < sources.size(); ++source) {
    if (stop.stop_requested()) { return std::unexpected(FootpathConnectionError::Canceled); }
    const auto &row = sources[source];
    if (row.Kind != FootpathKind::Path || !GroundFoot(row)) { continue; }
    double station = 0;
    for (size_t at = 1; at < row.Axis.size(); ++at) {
      const auto a = row.Axis[at - 1];
      const auto b = row.Axis[at];
      const double length = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
      index.Segments.push_back({.Source = static_cast<uint32_t>(source),
                                .Index = static_cast<uint32_t>(at - 1),
                                .StationM = station,
                                .LengthM = length});
      bounds.push_back(Bounds(a, b));
      station += length;
    }
  }
  auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy) { return std::unexpected(FootpathConnectionError::CapacityExceeded); }
  index.Hierarchy = std::move(*hierarchy);
  index.BuildHeapBytes = index.Hierarchy.HeapBytes() + bounds.capacity() * sizeof(Box) +
                         index.Segments.capacity() * sizeof(Segment);
  return index;
}

}
