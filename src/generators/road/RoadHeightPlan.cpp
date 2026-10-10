#include "RoadHeightPlan.h"

#include <algorithm>
#include <optional>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <vector>
#include <functional>
#include <numeric>
#include <queue>
#include <utility>

namespace outshine::Generators {
namespace {

constexpr double kContactToleranceM = 1e-8;
constexpr double kHeightToleranceM = 1e-10;
constexpr size_t kEarlyCycleSteps = 128;

struct Neighbor {
  uint32_t Node = 0;
  RoadHeightConstraintKind Kind = RoadHeightConstraintKind::ForwardLink;
  double RiseM = 0.0;
  double ReverseRiseM = 0.0;
  double BaseRiseM = 0.0;
  size_t Constraint = 0;
};

struct Neighbors {
  std::vector<size_t> First;
  std::vector<Neighbor> Connected;
};

struct HeightTrace {
  std::vector<size_t> Depth;
  std::vector<uint32_t> Parent;
  std::vector<size_t> Arc;
};

bool ValidNode(const RoadHeightNode &node) noexcept {
  return std::isfinite(node.LowSampleM) && std::isfinite(node.HighSampleM) &&
         node.LowSampleM <= node.HighSampleM && !std::isnan(node.MinimumM) &&
         !std::isnan(node.MaximumM) && node.MinimumM <= node.MaximumM &&
         node.MinimumM < std::numeric_limits<double>::infinity() &&
         node.MaximumM > -std::numeric_limits<double>::infinity();
}

Neighbors Connect(size_t nodes,
                  std::span<const RoadHeightLink> links,
                  std::span<const RoadHeightClearance> clearances) {
  Neighbors out{.First = std::vector<size_t>(nodes + 1), .Connected = {}};
  for (const auto &link : links) {
    ++out.First[link.First + 1];
    ++out.First[link.Second + 1];
  }
  for (const auto &clearance : clearances) {
    ++out.First[clearance.Lower + 1];
    ++out.First[clearance.Upper + 1];
  }
  std::partial_sum(out.First.begin(), out.First.end(), out.First.begin());
  out.Connected.resize(out.First.back());
  auto next = out.First;
  for (size_t at = 0; at < links.size(); ++at) {
    const auto &link = links[at];
    const double difference = link.FirstOffsetM - link.SecondOffsetM;
    out.Connected[next[link.First]++] = {.Node = link.Second,
                                         .RiseM = link.MaximumRiseM + difference,
                                         .ReverseRiseM = link.MaximumRiseM - difference,
                                         .BaseRiseM = link.MaximumRiseM,
                                         .Constraint = at};
    out.Connected[next[link.Second]++] = {.Node = link.First,
                                          .Kind = RoadHeightConstraintKind::ReverseLink,
                                          .RiseM = link.MaximumRiseM - difference,
                                          .ReverseRiseM = link.MaximumRiseM + difference,
                                          .BaseRiseM = link.MaximumRiseM,
                                          .Constraint = at};
  }
  for (size_t at = 0; at < clearances.size(); ++at) {
    const auto &clearance = clearances[at];
    const double bound = -clearance.MinimumGapM;
    const double rise = bound + clearance.UpperOffsetM - clearance.LowerOffsetM;
    out.Connected[next[clearance.Upper]++] = {.Node = clearance.Lower,
                                              .Kind = RoadHeightConstraintKind::Clearance,
                                              .RiseM = rise,
                                              .ReverseRiseM =
                                                  std::numeric_limits<double>::infinity(),
                                              .BaseRiseM = bound,
                                              .Constraint = at};
    out.Connected[next[clearance.Lower]++] = {.Node = clearance.Upper,
                                              .Kind = RoadHeightConstraintKind::Clearance,
                                              .RiseM = std::numeric_limits<double>::infinity(),
                                              .ReverseRiseM = rise,
                                              .BaseRiseM = bound,
                                              .Constraint = at};
  }
  return out;
}

std::unexpected<RoadHeightFailure> Failure(std::string_view reason) {
  return std::unexpected(
      RoadHeightFailure{.Reason = reason, .CycleNodes = {}, .CycleConstraints = {}});
}

RoadHeightConstraint TraversedConstraint(const Neighbor &arc, bool reverse) {
  auto kind = arc.Kind;
  if (reverse && kind != RoadHeightConstraintKind::Clearance) {
    kind = kind == RoadHeightConstraintKind::ForwardLink ? RoadHeightConstraintKind::ReverseLink
                                                         : RoadHeightConstraintKind::ForwardLink;
  }
  return {.Index = arc.Constraint, .Kind = kind};
}

std::optional<RoadHeightFailure> AttachmentConflict(uint32_t start,
                                                    std::span<const uint32_t> parent,
                                                    std::span<const size_t> parentArc,
                                                    const Neighbors &neighbors,
                                                    bool reverse,
                                                    size_t stepsMost) {
  constexpr uint32_t absent = std::numeric_limits<uint32_t>::max();
  uint32_t slow = start;
  uint32_t fast = start;
  for (size_t step = 0; step < stepsMost; ++step) {
    slow = parent[slow];
    fast = parent[fast];
    if (slow == absent || fast == absent) { return {}; }
    fast = parent[fast];
    if (fast == absent) { return {}; }
    if (slow != fast) { continue; }
    RoadHeightFailure failure{.Reason = "road profiles and clearance constraints conflict",
                              .CycleNodes = {},
                              .CycleConstraints = {}};
    double budgetM = 0;
    double costM = 0;
    uint32_t at = slow;
    do {
      failure.CycleNodes.push_back(at);
      const auto &arc = neighbors.Connected[parentArc[at]];
      failure.CycleConstraints.push_back(TraversedConstraint(arc, reverse));
      budgetM += arc.BaseRiseM;
      costM += reverse ? arc.ReverseRiseM : arc.RiseM;
      at = parent[at];
    } while (at != slow && failure.CycleNodes.size() <= parent.size());
    if (costM >= -kHeightToleranceM || at != slow) { return {}; }
    failure.MaximumOffsetScale =
        budgetM > 0 ? std::clamp(budgetM / (budgetM - costM), 0.0, 1.0) : 0.0;
    return failure;
  }
  return {};
}

std::optional<RoadHeightFailure> TraceConflict(const Neighbor &next,
                                               const HeightTrace &trace,
                                               const Neighbors &neighbors,
                                               bool reverse) {
  const bool fullWalk = trace.Depth[next.Node] >= trace.Parent.size();
  if (!fullWalk && (reverse ? next.ReverseRiseM : next.RiseM) >= 0) { return {}; }
  auto conflict = AttachmentConflict(next.Node,
                                     trace.Parent,
                                     trace.Arc,
                                     neighbors,
                                     reverse,
                                     fullWalk ? trace.Parent.size() : kEarlyCycleSteps);
  if (conflict) { return conflict; }
  if (fullWalk) {
    return RoadHeightFailure{.Reason = "road attachment cycle exceeded its work bound",
                             .CycleNodes = {},
                             .CycleConstraints = {}};
  }
  return {};
}

template <typename Seed>
std::expected<std::vector<double>, RoadHeightFailure> Envelope(
    std::span<const RoadHeightNode> nodes, const Neighbors &neighbors, Seed seed, bool reverse) {
  using Visit = std::pair<double, uint32_t>;
  std::vector<double> heights;
  heights.reserve(nodes.size());
  std::vector<Visit> initial;
  initial.reserve(nodes.size());
  for (uint32_t at = 0; at < nodes.size(); ++at) {
    heights.push_back(seed(at));
    if (std::isfinite(heights.back())) { initial.emplace_back(heights.back(), at); }
  }
  std::priority_queue<Visit, std::vector<Visit>, std::greater<>> frontier(std::greater<>{},
                                                                          std::move(initial));
  HeightTrace trace{.Depth = std::vector<size_t>(nodes.size()),
                    .Parent =
                        std::vector<uint32_t>(nodes.size(), std::numeric_limits<uint32_t>::max()),
                    .Arc = std::vector<size_t>(nodes.size())};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  size_t visits = 0;
  while (!frontier.empty()) {
    if ((++visits % 4096) == 0 && std::chrono::steady_clock::now() >= deadline) {
      return Failure("road height planning exceeded its work deadline");
    }
    const auto [height, node] = frontier.top();
    frontier.pop();
    if (height != heights[node]) { continue; }
    for (size_t at = neighbors.First[node]; at < neighbors.First[node + 1]; ++at) {
      const Neighbor &next = neighbors.Connected[at];
      const double riseM = reverse ? next.ReverseRiseM : next.RiseM;
      if (!std::isfinite(riseM)) { continue; }
      const double candidate = height + riseM;
      if (candidate >= heights[next.Node] - kHeightToleranceM) { continue; }
      trace.Depth[next.Node] = trace.Depth[node] + 1;
      trace.Parent[next.Node] = node;
      trace.Arc[next.Node] = at;
      const auto conflict = TraceConflict(next, trace, neighbors, reverse);
      if (conflict) { return std::unexpected(*conflict); }
      heights[next.Node] = candidate;
      frontier.emplace(candidate, next.Node);
    }
  }
  return heights;
}

std::expected<RoadHeightPlan, RoadHeightFailure> CutHeights(std::span<const RoadHeightNode> nodes,
                                                            const Neighbors &neighbors,
                                                            std::span<const double> floor,
                                                            std::span<const double> ceiling) {
  for (size_t at = 0; at < nodes.size(); ++at) {
    if (-floor[at] > ceiling[at] + kContactToleranceM) {
      return Failure("road contacts conflict with the permitted gradients");
    }
  }
  auto heights = Envelope(
      nodes,
      neighbors,
      [&](uint32_t at) {
        return std::min(ceiling[at], std::max(-floor[at], nodes[at].LowSampleM));
      },
      false);
  if (!heights) { return std::unexpected(heights.error()); }
  RoadHeightPlan plan;
  plan.HeightM = std::move(*heights);
  for (size_t at = 0; at < nodes.size(); ++at) {
    const double heightM = plan.HeightM[at];
    plan.MaximumAdjustmentM = std::max({plan.MaximumAdjustmentM,
                                        std::abs(heightM - nodes[at].LowSampleM),
                                        std::abs(heightM - nodes[at].HighSampleM)});
  }
  if (!std::isfinite(plan.MaximumAdjustmentM)) {
    return Failure("road height adjustment exceeds its numeric range");
  }
  return plan;
}

}

std::expected<RoadHeightPlan, RoadHeightFailure>
PlanRoadHeights(std::span<const RoadHeightNode> nodes,
                std::span<const RoadHeightLink> links,
                RoadHeightFit fit) {
  return PlanRoadHeights(nodes, links, {}, fit);
}

std::expected<RoadHeightPlan, RoadHeightFailure>
PlanRoadHeights(std::span<const RoadHeightNode> nodes,
                std::span<const RoadHeightLink> links,
                std::span<const RoadHeightClearance> clearances,
                RoadHeightFit fit) {
  if (nodes.size() > std::numeric_limits<uint32_t>::max() ||
      !std::ranges::all_of(nodes, ValidNode) ||
      !std::ranges::all_of(links,
                           [&](const auto &link) {
                             return link.First < nodes.size() && link.Second < nodes.size() &&
                                    std::isfinite(link.MaximumRiseM) && link.MaximumRiseM >= 0.0;
                           }) ||
      !std::ranges::all_of(links,
                           [](const auto &link) {
                             const double difference = link.FirstOffsetM - link.SecondOffsetM;
                             return std::isfinite(difference) &&
                                    std::isfinite(link.MaximumRiseM + difference) &&
                                    std::isfinite(link.MaximumRiseM - difference);
                           }) ||
      !std::ranges::all_of(clearances, [&](const auto &clearance) {
        const double rise =
            -clearance.MinimumGapM + clearance.UpperOffsetM - clearance.LowerOffsetM;
        return clearance.Lower < nodes.size() && clearance.Upper < nodes.size() &&
               std::isfinite(clearance.MinimumGapM) && clearance.MinimumGapM >= 0 &&
               std::isfinite(rise);
      })) {
    return Failure("invalid road height constraints");
  }
  const auto neighbors = Connect(nodes.size(), links, clearances);
  const auto floor =
      Envelope(nodes, neighbors, [&](uint32_t at) { return -nodes[at].MinimumM; }, true);
  if (!floor) { return std::unexpected(floor.error()); }
  const auto ceiling =
      Envelope(nodes, neighbors, [&](uint32_t at) { return nodes[at].MaximumM; }, false);
  if (!ceiling) { return std::unexpected(ceiling.error()); }
  if (fit == RoadHeightFit::PreferCuts) { return CutHeights(nodes, neighbors, *floor, *ceiling); }
  const auto upper =
      Envelope(nodes, neighbors, [&](uint32_t at) { return nodes[at].LowSampleM; }, false);
  if (!upper) { return std::unexpected(upper.error()); }
  const auto lower =
      Envelope(nodes, neighbors, [&](uint32_t at) { return -nodes[at].HighSampleM; }, true);
  if (!lower) { return std::unexpected(lower.error()); }
  RoadHeightPlan plan;
  for (size_t at = 0; at < nodes.size(); ++at) {
    if (-(*floor)[at] > (*ceiling)[at] + kContactToleranceM) {
      return Failure("road contacts conflict with the permitted gradients");
    }
    plan.MaximumAdjustmentM = std::max({plan.MaximumAdjustmentM,
                                        -(*lower)[at] * 0.5 - (*upper)[at] * 0.5,
                                        -(*floor)[at] - (*upper)[at],
                                        -(*lower)[at] - (*ceiling)[at]});
  }
  if (!std::isfinite(plan.MaximumAdjustmentM)) {
    return Failure("road height adjustment exceeds its numeric range");
  }
  plan.HeightM.reserve(nodes.size());
  for (size_t at = 0; at < nodes.size(); ++at) {
    const double low = std::max(-(*floor)[at], -(*lower)[at] - plan.MaximumAdjustmentM);
    const double high = std::min((*ceiling)[at], (*upper)[at] + plan.MaximumAdjustmentM);
    const double reference = std::midpoint(-(*lower)[at], (*upper)[at]);
    plan.HeightM.push_back(std::min(high, std::max(low, reference)));
  }
  return plan;
}

}
