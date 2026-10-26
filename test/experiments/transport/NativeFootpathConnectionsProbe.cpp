#include "FootpathConnectionPlan.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <print>
#include <ratio>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

bool Read(FootpathSource &source, std::vector<EastNorth> &points) {
  unsigned kind = 0;
  unsigned first = 0;
  unsigned last = 0;
  int layerKnown = 0;
  int layer = 0;
  int bridge = 0;
  int tunnel = 0;
  int foot = 0;
  int boundKnown = 0;
  double bound = 0;
  size_t count = 0;
  if (!(std::cin >> kind >> layerKnown >> layer >> bridge >> tunnel >> foot >> first >> last >>
        source.HalfWidthM >> boundKnown >> bound >> count) ||
      kind > 2 || first > 4 || last > 4 || count < 2 || count > 4096 || bridge < -1 || bridge > 1 ||
      tunnel < -1 || tunnel > 1 || foot < -1 || foot > 1) {
    return false;
  }
  source.Kind = static_cast<FootpathKind>(kind);
  source.Ends = {static_cast<FootpathEndRole>(first), static_cast<FootpathEndRole>(last)};
  if (layerKnown != 0) { source.Layer = layer; }
  if (bridge >= 0) { source.Bridge = bridge != 0; }
  if (tunnel >= 0) { source.Tunnel = tunnel != 0; }
  if (foot >= 0) { source.FootAccess = foot != 0; }
  if (boundKnown != 0) { source.PositionErrorBoundM = bound; }
  points.resize(count);
  for (auto &point : points) {
    if (!(std::cin >> point.EastM >> point.NorthM)) { return false; }
  }
  source.Axis = points;
  return true;
}
}

int main() {
  using Clock = std::chrono::steady_clock;
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  constexpr size_t kMaximumSources = 200000;
  size_t count = 0;
  FootpathConnectionRecipe recipe;
  FootpathConnectionBudget budget;
  if (!(std::cin >> count >> recipe.ReachM >> recipe.MinimumCrossingSine >> budget.Segments >>
        budget.Endpoints >> budget.Candidates) ||
      count > kMaximumSources) {
    return 2;
  }
  std::vector<FootpathSource> sources(count);
  std::vector<std::vector<EastNorth>> points(count);
  for (size_t source = 0; source < count; ++source) {
    if (!Read(sources[source], points[source])) { return 2; }
  }
  const auto began = Clock::now();
  const auto plan = FootpathConnectionPlan::Prepare(sources, recipe, budget);
  const auto prepared = Clock::now();
  if (!plan) {
    std::println("error {}", static_cast<unsigned>(plan.error()));
    return 1;
  }
  std::vector<std::vector<FootpathAxisVertex>> axes;
  axes.reserve(count);
  for (size_t source = 0; source < count; ++source) {
    axes.push_back(plan->Axis(static_cast<uint32_t>(source), points[source]));
  }
  const double planMs = std::chrono::duration<double, std::milli>(prepared - began).count();
  const double axesMs = std::chrono::duration<double, std::milli>(Clock::now() - prepared).count();
  const auto &work = plan->Work();
  std::println("plan {} {} {} {} {} {} {} {} {} {} {:.17g} {:.17g}",
               plan->Connections().size(),
               plan->Endpoints().size(),
               work.SourceSegments,
               work.IndexedSegments,
               work.Queries,
               work.CandidateTests,
               work.MaximumCandidates,
               work.BudgetRefusals,
               work.IndexBuildHeapBytes,
               work.ScratchHeapBytes,
               planMs,
               axesMs);
  for (const auto &connection : plan->Connections()) {
    std::println("connection {} {} {} {} {:.17g} {:.17g} {:.17g} {:.17g} {:.17g} {:.17g} {}",
                 connection.Branch,
                 connection.End,
                 connection.Target,
                 connection.Segment,
                 connection.TargetFraction,
                 connection.TargetStationM,
                 connection.JoinedAt.EastM,
                 connection.JoinedAt.NorthM,
                 connection.AdvanceM,
                 connection.CrossingSine,
                 static_cast<unsigned>(connection.PositionBoundsKnown));
  }
  for (const auto &endpoint : plan->Endpoints()) {
    std::println("endpoint {} {} {}",
                 endpoint.Source,
                 endpoint.End,
                 static_cast<unsigned>(endpoint.Disposition));
  }
  for (size_t source = 0; source < axes.size(); ++source) {
    std::println("axis {} {}", source, axes[source].size());
    for (const auto point : axes[source]) {
      std::println("{:.17g} {:.17g}", point.At.EastM, point.At.NorthM);
    }
  }
}
