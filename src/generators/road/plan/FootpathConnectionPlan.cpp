#include "FootpathConnectionInternal.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

#include "Earth.h"
#include "math/Box.h"

namespace outshine::Generators {
namespace {

bool Query(const FootpathConnections::Index &index,
           EastNorth point,
           FootpathConnectionRecipe recipe,
           size_t remaining,
           std::vector<uint32_t> &candidates) {
  const Box region{.Min = {{point.EastM - recipe.ReachM, point.NorthM - recipe.ReachM, 0}},
                   .Max = {{point.EastM + recipe.ReachM, point.NorthM + recipe.ReachM, 0}}};
  candidates.clear();
  const bool complete = index.Hierarchy.Query(region, [&](uint32_t source) {
    candidates.push_back(source);
    return candidates.size() <= remaining;
  });
  if (complete) { std::ranges::sort(candidates); }
  return complete;
}

struct AxisCut {
  uint32_t Segment = 0;
  double Fraction = 0;
  EastNorth At;
  uint32_t Connection = 0;
};

}

std::expected<FootpathConnectionPlan, FootpathConnectionError>
FootpathConnectionPlan::Prepare(std::span<const FootpathSource> sources,
                                FootpathConnectionRecipe recipe,
                                FootpathConnectionBudget budget,
                                const std::stop_token &stop) {
  using namespace FootpathConnections;
  if (stop.stop_requested()) { return std::unexpected(FootpathConnectionError::Canceled); }
  auto inputs = PrepareInputs(sources, recipe, budget, stop);
  if (!inputs) { return std::unexpected(inputs.error()); }
  FootpathConnectionPlan plan;
  plan.Work_.SourceSegments = inputs->Segments;
  std::optional<Index> index;
  if (inputs->EligibleEndpoints != 0) {
    auto built = BuildIndex(sources, stop);
    if (!built) { return std::unexpected(built.error()); }
    index = std::move(*built);
    plan.Work_.IndexedSegments = index->Segments.size();
    plan.Work_.IndexHeapBytes =
        index->Hierarchy.HeapBytes() + index->Segments.capacity() * sizeof(Segment);
    plan.Work_.IndexBuildHeapBytes = index->BuildHeapBytes;
  }
  const auto found =
      plan.FindConnections(sources, *inputs, index ? &*index : nullptr, recipe, budget, stop);
  if (!found) { return std::unexpected(found.error()); }
  return plan;
}

std::expected<void, FootpathConnectionError>
FootpathConnectionPlan::FindConnections(std::span<const FootpathSource> sources,
                                        const FootpathConnections::Inputs &inputs,
                                        const FootpathConnections::Index *index,
                                        FootpathConnectionRecipe recipe,
                                        FootpathConnectionBudget budget,
                                        const std::stop_token &stop) {
  using namespace FootpathConnections;
  Endpoints_.reserve(inputs.Endpoints);
  std::vector<uint32_t> candidates;
  for (size_t source = 0; source < sources.size(); ++source) {
    const auto &row = sources[source];
    if (row.Kind != FootpathKind::Steps) { continue; }
    const auto found = FindEndpoints(
        static_cast<uint32_t>(source), sources, inputs, index, recipe, budget, candidates, stop);
    if (!found) { return std::unexpected(found.error()); }
  }

  for (size_t at = 0; at < Connections_.size(); ++at) {
    const auto &connection = Connections_[at];
    Edits_.push_back({.Source = connection.Branch, .Connection = static_cast<uint32_t>(at)});
    Edits_.push_back({.Source = connection.Target, .Connection = static_cast<uint32_t>(at)});
  }
  std::ranges::sort(Edits_, {}, &FootpathAxisEdit::Source);
  Work_.ScratchHeapBytes =
      inputs.Lengths.capacity() * sizeof(double) + candidates.capacity() * sizeof(uint32_t);
  return {};
}

std::expected<void, FootpathConnectionError>
FootpathConnectionPlan::FindEndpoints(uint32_t source,
                                      std::span<const FootpathSource> sources,
                                      const FootpathConnections::Inputs &inputs,
                                      const FootpathConnections::Index *index,
                                      FootpathConnectionRecipe recipe,
                                      FootpathConnectionBudget budget,
                                      std::vector<uint32_t> &candidates,
                                      const std::stop_token &stop) {
  using namespace FootpathConnections;
  const auto &row = sources[source];
  for (uint32_t end = 0; end < 2; ++end) {
    if (stop.stop_requested()) { return std::unexpected(FootpathConnectionError::Canceled); }
    auto disposition = Disposition(row, end);
    if (disposition == FootpathEndDisposition::NoTarget && index != nullptr) {
      ++Work_.Queries;
      const auto point = end == 0 ? row.Axis.front() : row.Axis.back();
      const bool complete =
          Query(*index, point, recipe, budget.Candidates - Work_.CandidateTests, candidates);
      Work_.MaximumCandidates = std::max(Work_.MaximumCandidates, candidates.size());
      if (complete) {
        Work_.CandidateTests += candidates.size();
        auto chosen = Choose(sources, inputs, *index, source, end, candidates, recipe, disposition);
        if (chosen) { Connections_.push_back(*chosen); }
      } else {
        ++Work_.BudgetRefusals;
        disposition = FootpathEndDisposition::CandidateBudgetExceeded;
      }
    }
    Endpoints_.push_back({.Source = source, .End = end, .Disposition = disposition});
  }
  return {};
}

std::span<const FootpathAxisEdit> FootpathConnectionPlan::EditsOf(uint32_t source) const {
  const auto range = std::ranges::equal_range(Edits_, source, {}, &FootpathAxisEdit::Source);
  return {range.begin(), range.end()};
}

std::vector<FootpathAxisVertex>
FootpathConnectionPlan::Axis(uint32_t source, std::span<const EastNorth> original) const {
  std::array<std::optional<uint32_t>, 2> ends{};
  std::vector<AxisCut> cuts;
  for (const auto &edit : EditsOf(source)) {
    const auto &connection = Connections_[edit.Connection];
    if (connection.Branch == source) { ends[connection.End] = edit.Connection; }
    if (connection.Target == source) {
      cuts.push_back({.Segment = connection.Segment,
                      .Fraction = connection.TargetFraction,
                      .At = connection.JoinedAt,
                      .Connection = edit.Connection});
    }
  }
  std::ranges::sort(cuts, [](const AxisCut &a, const AxisCut &b) {
    return a.Segment == b.Segment ? a.Fraction < b.Fraction : a.Segment < b.Segment;
  });
  std::vector<FootpathAxisVertex> axis;
  axis.reserve(original.size() + cuts.size() + 2);
  const auto append = [&](FootpathAxisVertex vertex) {
    if (axis.empty() || axis.back().At != vertex.At) {
      axis.push_back(vertex);
      return;
    }
    if (vertex.Original != FootpathAxisVertex::kNoIndex) { axis.back().Original = vertex.Original; }
    if (axis.back().Connection == FootpathAxisVertex::kNoIndex) {
      axis.back().Connection = vertex.Connection;
    }
  };
  const auto &first = ends.front();
  if (first) { append({.At = Connections_[*first].JoinedAt, .Connection = *first}); }
  size_t cut = 0;
  for (size_t at = 0; at < original.size(); ++at) {
    append({.At = original[at], .Original = static_cast<uint32_t>(at)});
    while (cut < cuts.size() && cuts[cut].Segment == at) {
      append({.At = cuts[cut].At, .Connection = cuts[cut].Connection});
      ++cut;
    }
  }
  const auto &last = ends.back();
  if (last) { append({.At = Connections_[*last].JoinedAt, .Connection = *last}); }
  return axis;
}

}
