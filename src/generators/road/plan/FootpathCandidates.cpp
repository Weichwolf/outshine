#include "FootpathConnectionInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

#include "Earth.h"

namespace outshine::Generators::FootpathConnections {
namespace {

EastNorth Difference(EastNorth a, EastNorth b) {
  return {.EastM = a.EastM - b.EastM, .NorthM = a.NorthM - b.NorthM};
}

double Cross(EastNorth a, EastNorth b) {
  return a.EastM * b.NorthM - a.NorthM * b.EastM;
}

}

std::optional<FootpathConnection> Candidate(std::span<const FootpathSource> sources,
                                            std::span<const double> lengths,
                                            uint32_t branch,
                                            uint32_t end,
                                            const Segment &segment,
                                            FootpathConnectionRecipe recipe) {
  const auto &source = sources[branch];
  const auto &target = sources[segment.Source];
  if (branch == segment.Source || source.Layer != target.Layer) { return std::nullopt; }
  const auto point = end == 0 ? source.Axis.front() : source.Axis.back();
  const auto behind = end == 0 ? source.Axis[1] : source.Axis[source.Axis.size() - 2];
  auto unit = Difference(point, behind);
  const double magnitude = std::hypot(unit.EastM, unit.NorthM);
  unit.EastM /= magnitude;
  unit.NorthM /= magnitude;
  const auto a = target.Axis[segment.Index];
  const auto b = target.Axis[segment.Index + 1];
  const auto axis = Difference(b, a);
  const auto delta = Difference(a, point);
  const double denominator = Cross(unit, axis);
  if (denominator == 0) { return std::nullopt; }
  const double sine = std::abs(denominator) / segment.LengthM;
  if (sine < recipe.MinimumCrossingSine) { return std::nullopt; }
  const double advance = Cross(delta, axis) / denominator;
  const double fraction = Cross(delta, unit) / denominator;
  double limit = std::min(recipe.ReachM, target.HalfWidthM);
  const bool known =
      source.PositionErrorBoundM.has_value() && target.PositionErrorBoundM.has_value();
  if (known) { limit = std::min(limit, *source.PositionErrorBoundM + *target.PositionErrorBoundM); }
  if (!std::isfinite(advance) || !std::isfinite(fraction) || advance < 0 || advance > limit ||
      fraction < 0 || fraction > 1) {
    return std::nullopt;
  }
  const double station = segment.StationM + fraction * segment.LengthM;
  if (std::min(station, lengths[segment.Source] - station) < source.HalfWidthM) {
    return std::nullopt;
  }
  EastNorth joined = a;
  if (fraction == 1) {
    joined = b;
  } else if (fraction != 0) {
    joined = {.EastM = a.EastM + fraction * axis.EastM,
              .NorthM = a.NorthM + fraction * axis.NorthM};
  }
  return FootpathConnection{.Branch = branch,
                            .End = end,
                            .Target = segment.Source,
                            .Segment = segment.Index,
                            .TargetFraction = fraction,
                            .TargetStationM = station,
                            .JoinedAt = joined,
                            .AdvanceM = advance,
                            .CrossingSine = sine,
                            .PositionBoundsKnown = known};
}

std::optional<FootpathConnection> Choose(std::span<const FootpathSource> sources,
                                         const Inputs &inputs,
                                         const Index &index,
                                         uint32_t branch,
                                         uint32_t end,
                                         std::span<const uint32_t> candidates,
                                         FootpathConnectionRecipe recipe,
                                         FootpathEndDisposition &disposition) {
  std::optional<FootpathConnection> chosen;
  bool coincident = false;
  bool ambiguous = false;
  for (const auto at : candidates) {
    auto next = Candidate(sources, inputs.Lengths, branch, end, index.Segments[at], recipe);
    if (!next) { continue; }
    if (coincident && next->AdvanceM != 0) { continue; }
    if (next->AdvanceM == 0 && !coincident) {
      chosen.reset();
      ambiguous = false;
      coincident = true;
    }
    if (!chosen) {
      chosen = *next;
    } else if (chosen->Target != next->Target || chosen->JoinedAt != next->JoinedAt) {
      ambiguous = true;
    }
  }
  disposition = chosen ? FootpathEndDisposition::Proposed : FootpathEndDisposition::NoTarget;
  if (ambiguous) {
    disposition = FootpathEndDisposition::AmbiguousTargets;
    chosen.reset();
  }
  return chosen;
}

}
