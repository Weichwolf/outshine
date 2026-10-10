#include "Corridors.h"
#include "RoadHeightPlan.h"
#include "JunctionSlopePlan.h"
#include "Heap.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <ratio>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace outshine::Generators {
namespace {

struct HeightAttachment {
  uint64_t Node = 0;
  double OffsetM = 0.0;
  uint32_t Junction = std::numeric_limits<uint32_t>::max();
};

struct ProfileLimits {
  double MaxGradient = 0.0;
  double MinimumM = 0.0;
};

struct SlopeAttachment {
  uint32_t Station = 0;
  double Weight = 1.0;
};

}

struct Corridors::HeightGraph {
  std::vector<RoadHeightNode> Nodes;
  std::vector<RoadHeightLink> Links;
  std::vector<RoadHeightClearance> Clearances;
  std::vector<uint32_t> Stations;
  std::vector<double> OffsetsM;
  std::vector<uint32_t> StationPlanes;
  std::vector<uint32_t> LinkStations;
  std::vector<JunctionSlopeConstraint> PlaneConstraints;
  std::unordered_map<uint64_t, uint32_t> Named;
  std::unordered_map<uint64_t, uint64_t> HeightOwners;
  std::unordered_map<uint64_t, uint32_t> JunctionAt;
  std::vector<bool> Adjusted;
  size_t AdjustedJunctions = 0;
};

namespace {

constexpr double kStationChangeToleranceM = .01;
constexpr double kPlaneClearanceMarginM = 1e-8;
constexpr size_t kPlaneAttemptsMost = 64;
constexpr auto kPlaneTimeBudget = std::chrono::seconds(5);

uint64_t HeightOwner(uint64_t node, const auto &graph) {
  for (;;) {
    const auto found = graph.HeightOwners.find(node);
    if (found == graph.HeightOwners.end() || found->second == node) { return node; }
    node = found->second;
  }
}

void BindCrossingHeights(const auto &on, const auto &into, auto &graph) {
  graph.HeightOwners.reserve(2 * into.Crossings.size());
  for (const auto &crossing : into.Crossings) {
    const auto &a = on.Ways.Ways()[crossing.Lanes[0]];
    const auto &b = on.Ways.Ways()[crossing.Lanes[1]];
    if (a.Layer != b.Layer || a.Bridge != b.Bridge) { continue; }
    const uint64_t first = HeightOwner(crossing.Nodes[0], graph);
    const uint64_t second = HeightOwner(crossing.Nodes[1], graph);
    const uint64_t owner = std::min(first, second);
    graph.HeightOwners.insert_or_assign(first, owner);
    graph.HeightOwners.insert_or_assign(second, owner);
  }
}

uint32_t
HeightNode(const RoadStation &station, HeightAttachment attachment, double minimumM, auto &graph) {
  auto node = static_cast<uint32_t>(graph.Nodes.size());
  if (attachment.Node != 0) {
    const uint64_t owner = HeightOwner(attachment.Node, graph);
    const auto [found, inserted] = graph.Named.try_emplace(owner, node);
    if (!inserted) { node = found->second; }
    if (owner != attachment.Node) { graph.Named.insert_or_assign(attachment.Node, node); }
  }
  const double sampleM = station.GradeM - attachment.OffsetM;
  const double floorM = minimumM - attachment.OffsetM;
  if (node == graph.Nodes.size()) {
    graph.Nodes.push_back({.LowSampleM = sampleM, .HighSampleM = sampleM, .MinimumM = floorM});
  } else {
    auto &held = graph.Nodes[node];
    held.LowSampleM = std::min(held.LowSampleM, sampleM);
    held.HighSampleM = std::max(held.HighSampleM, sampleM);
    held.MinimumM = std::max(held.MinimumM, floorM);
  }
  return node;
}

void AppendProfile(std::span<const RoadStation> along,
                   const std::array<HeightAttachment, 2> &ends,
                   ProfileLimits limits,
                   auto &graph) {
  uint32_t previous = 0;
  double previousOffsetM = 0.0;
  for (size_t at = 0; at < along.size(); ++at) {
    HeightAttachment attachment{.Node = along[at].Node};
    if (at == 0) {
      attachment = ends[0];
    } else if (at + 1 == along.size()) {
      attachment = ends[1];
    }
    const uint32_t node = HeightNode(along[at], attachment, limits.MinimumM, graph);
    graph.Stations.push_back(node);
    graph.OffsetsM.push_back(attachment.OffsetM);
    graph.StationPlanes.push_back(attachment.Junction);
    if (at > 0) {
      const double runM = std::hypot(along[at].EastM - along[at - 1].EastM,
                                     along[at].NorthM - along[at - 1].NorthM);
      graph.Links.push_back(
          {previous, node, limits.MaxGradient * runM, previousOffsetM, attachment.OffsetM});
      graph.LinkStations.push_back(static_cast<uint32_t>(graph.Stations.size() - 2));
    }
    previous = node;
    previousOffsetM = attachment.OffsetM;
  }
}

void AppendSlopeTerm(SlopeAttachment from,
                     const auto &into,
                     const auto &graph,
                     JunctionSlopeConstraint &constraint) {
  const uint32_t plane = graph.StationPlanes[from.Station];
  if (plane == std::numeric_limits<uint32_t>::max()) { return; }
  auto term = std::ranges::find_if(constraint.Terms,
                                   [&](const auto &one) { return one.Junction == plane; });
  if (term == constraint.Terms.end()) {
    constraint.Terms.push_back({.Junction = plane});
    term = constraint.Terms.end() - 1;
  }
  const auto &at = into.Planned[from.Station];
  const auto &junction = into.Junctions[plane];
  term->EastM += from.Weight * (at.EastM - junction.EastM);
  term->NorthM += from.Weight * (at.NorthM - junction.NorthM);
}

JunctionSlopeConstraint
SlopeConstraint(const RoadHeightFailure &failure, const auto &into, const auto &graph) {
  JunctionSlopeConstraint constraint{.Terms = {}, .MinimumRiseM = kPlaneClearanceMarginM};
  for (const auto &crossed : failure.CycleConstraints) {
    if (crossed.Kind == RoadHeightConstraintKind::Clearance) {
      constraint.MinimumRiseM += graph.Clearances[crossed.Index].MinimumGapM;
      continue;
    }
    const auto &link = graph.Links[crossed.Index];
    constraint.MinimumRiseM -= link.MaximumRiseM;
    const uint32_t first = graph.LinkStations[crossed.Index];
    const double sign = crossed.Kind == RoadHeightConstraintKind::ForwardLink ? 1.0 : -1.0;
    AppendSlopeTerm({.Station = first, .Weight = sign}, into, graph, constraint);
    AppendSlopeTerm({.Station = first + 1, .Weight = -sign}, into, graph, constraint);
  }
  return constraint;
}

std::expected<void, std::string_view>
AdjustJunctionPlanes(const RoadHeightFailure &failure,
                     auto &into,
                     auto &graph,
                     std::chrono::steady_clock::time_point deadline) {
  graph.PlaneConstraints.push_back(SlopeConstraint(failure, into, graph));
  std::vector<JunctionSlope> slopes;
  slopes.reserve(into.Junctions.size());
  for (const auto &junction : into.Junctions) {
    slopes.push_back({junction.SlopeE, junction.SlopeN, junction.MaxGradient});
  }
  if (auto plan = FitJunctionSlopes(slopes, graph.PlaneConstraints, deadline); !plan) {
    return plan;
  }
  for (size_t at = 0; at < into.Junctions.size(); ++at) {
    auto &junction = into.Junctions[at];
    if (junction.SlopeE == slopes[at].East && junction.SlopeN == slopes[at].North) { continue; }
    junction.SlopeE = slopes[at].East;
    junction.SlopeN = slopes[at].North;
    if (!graph.Adjusted[at]) { ++graph.AdjustedJunctions; }
    graph.Adjusted[at] = true;
  }
  return {};
}

}

void Corridors::PrepareProfile(size_t edgeAt, const Paving &on, Paved &into) {
  Edge &edge = into.Edges[edgeAt];
  const RoadStation *const first = into.Designed[edge.Lane].data() + edge.First;
  into.Along.assign(first, first + edge.Count);
  const auto began = std::chrono::steady_clock::now();
  static const Heap::Tag kFittingTag("road-fit");
  const Heap::Tagged fitting(kFittingTag);
  FitLane(edge, into);
  into.FitMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (into.Along.size() < 2) {
    ++into.RefusedWays;
    return;
  }
  const auto &way = on.Ways.Ways()[edge.Lane];
  const double firstM = edge.HasEndGrade[0] ? edge.GradeAtM[0] : into.Along.front().GradeM;
  const double lastM = edge.HasEndGrade[1] ? edge.GradeAtM[1] : into.Along.back().GradeM;
  if (way.Bridge) {
    const auto reached = ReachedAlong(into.Along);
    const double floorM = into.DeckM[edge.Lane];
    if (reached.back() > 0.0) {
      for (size_t at = 0; at < into.Along.size(); ++at) {
        into.Along[at].GradeM = std::lerp(
            std::max(floorM, firstM), std::max(floorM, lastM), reached[at] / reached.back());
      }
    }
  } else {
    into.Along.front().GradeM = firstM;
    into.Along.back().GradeM = lastM;
  }
  edge.PlannedFirst = static_cast<uint32_t>(into.Planned.size());
  edge.PlannedCount = static_cast<uint32_t>(into.Along.size());
  into.Planned.insert(into.Planned.end(), into.Along.begin(), into.Along.end());
}

void Corridors::CrossingClearances(const Paving &on, const Paved &into, HeightGraph &graph) {
  for (const auto &crossing : into.Crossings) {
    if (crossing.Nodes[0] == crossing.Nodes[1]) { continue; }
    const auto &first = on.Ways.Ways()[crossing.Lanes[0]];
    const auto &second = on.Ways.Ways()[crossing.Lanes[1]];
    if (first.Layer == second.Layer && first.Bridge == second.Bridge) { continue; }
    if (!first.Bridge && !second.Bridge) { continue; }
    const bool firstAbove = first.Layer != second.Layer ? first.Layer > second.Layer : first.Bridge;
    const size_t upper = firstAbove ? 0 : 1;
    const size_t lower = 1 - upper;
    const auto above = graph.Named.find(crossing.Nodes[upper]);
    const auto below = graph.Named.find(crossing.Nodes[lower]);
    if (above == graph.Named.end() || below == graph.Named.end()) { continue; }
    const auto &upperWay = on.Ways.Ways()[crossing.Lanes[upper]];
    const auto &lowerWay = on.Ways.Ways()[crossing.Lanes[lower]];
    if (lowerWay.Bridge) {
      auto &node = graph.Nodes[below->second];
      node.MinimumM = std::max(node.MinimumM, crossing.GradeM + kSealedDepthM);
    }
    graph.Clearances.push_back(
        {.Lower = below->second,
         .Upper = above->second,
         .MinimumGapM = lowerWay.ClearanceM + (upperWay.Bridge ? kSealedDepthM : 0)});
  }
}

void Corridors::HeightConstraints(const Paving &on, Paved &into, HeightGraph &graph) {
  graph.Nodes.clear();
  graph.Links.clear();
  graph.Clearances.clear();
  graph.Named.clear();
  graph.Stations.clear();
  graph.OffsetsM.clear();
  graph.StationPlanes.clear();
  graph.LinkStations.clear();
  for (const auto &junction : into.Junctions) {
    const RoadStation centre{.GradeM = junction.GradeM, .Node = junction.Node};
    (void)HeightNode(
        centre, {.Node = junction.Node}, -std::numeric_limits<double>::infinity(), graph);
  }
  for (const auto &edge : into.Edges) {
    if (edge.PlannedCount < 2) { continue; }
    const auto along = std::span(into.Planned).subspan(edge.PlannedFirst, edge.PlannedCount);
    std::array<HeightAttachment, 2> ends{{{.Node = edge.NodeAt[0]}, {.Node = edge.NodeAt[1]}}};
    for (size_t end = 0; end < 2; ++end) {
      const auto found = graph.JunctionAt.find(ends[end].Node);
      if (found == graph.JunctionAt.end()) { continue; }
      const auto &junction = into.Junctions[found->second];
      ends[end].Junction = found->second;
      auto &at = end == 0 ? along.front() : along.back();
      ends[end].OffsetM = junction.SlopeE * (at.EastM - junction.EastM) +
                          junction.SlopeN * (at.NorthM - junction.NorthM);
      at.GradeM = junction.GradeM + ends[end].OffsetM;
    }
    const auto &way = on.Ways.Ways()[edge.Lane];
    const double minimumM = way.Bridge && into.DeckM[edge.Lane] > kUnraisedDeckM
                                ? into.DeckM[edge.Lane]
                                : -std::numeric_limits<double>::infinity();
    AppendProfile(along, ends, {.MaxGradient = RoadGradient(way), .MinimumM = minimumM}, graph);
  }
  CrossingClearances(on, into, graph);
}

std::expected<void, std::string_view> Corridors::PlanHeights(const Paving &on, Paved &into) {
  const auto began = std::chrono::steady_clock::now();
  const size_t maximum = into.Planned.size() + into.Junctions.size();
  if (maximum > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("road height plan exceeds its station range");
  }
  HeightGraph graph;
  graph.Nodes.reserve(maximum);
  graph.Links.reserve(into.Planned.size());
  graph.Clearances.reserve(into.Crossings.size());
  graph.Stations.reserve(into.Planned.size());
  graph.OffsetsM.reserve(into.Planned.size());
  graph.StationPlanes.reserve(into.Planned.size());
  graph.LinkStations.reserve(into.Planned.size());
  graph.Named.reserve(into.Edges.size() * 2);
  graph.JunctionAt.reserve(into.Junctions.size());
  for (uint32_t at = 0; at < into.Junctions.size(); ++at) {
    graph.JunctionAt.emplace(into.Junctions[at].Node, at);
  }
  graph.Adjusted.resize(into.Junctions.size());
  BindCrossingHeights(on, into, graph);
  HeightConstraints(on, into, graph);
  auto plan =
      PlanRoadHeights(graph.Nodes, graph.Links, graph.Clearances, RoadHeightFit::PreferCuts);
  for (size_t attempt = 0; !plan; ++attempt) {
    const auto &failure = plan.error();
    if (failure.CycleNodes.empty()) { return std::unexpected(failure.Reason); }
    if (attempt >= kPlaneAttemptsMost ||
        std::chrono::steady_clock::now() - began >= kPlaneTimeBudget) {
      return std::unexpected("road junction plane planning exceeded its work bound");
    }
    if (auto adjusted = AdjustJunctionPlanes(failure, into, graph, began + kPlaneTimeBudget);
        !adjusted) {
      return adjusted;
    }
    HeightConstraints(on, into, graph);
    plan = PlanRoadHeights(graph.Nodes, graph.Links, graph.Clearances, RoadHeightFit::PreferCuts);
  }
  size_t changedStations = 0;
  for (size_t at = 0; at < into.Planned.size(); ++at) {
    const double heightM = plan->HeightM[graph.Stations[at]] + graph.OffsetsM[at];
    if (std::abs(heightM - into.Planned[at].GradeM) > kStationChangeToleranceM) {
      ++changedStations;
    }
    into.Planned[at].GradeM = heightM;
  }
  into.SteepestJunction = 0;
  into.MostOffGroundM = 0;
  for (auto &junction : into.Junctions) {
    junction.GradeM = plan->HeightM[graph.Named.at(junction.Node)];
    into.SteepestJunction =
        std::max(into.SteepestJunction, std::hypot(junction.SlopeE, junction.SlopeN));
    into.MostOffGroundM =
        std::max(into.MostOffGroundM, std::abs(junction.GradeM - junction.RootsM));
    for (auto &gate : junction.Gates) {
      gate.GradeM = junction.GradeM + junction.SlopeE * (gate.EastM - junction.EastM) +
                    junction.SlopeN * (gate.NorthM - junction.NorthM);
    }
  }
  Notes(into, "streets: joint height nodes", static_cast<double>(graph.Nodes.size()), "nodes");
  Notes(into,
        "streets: planned level clearances",
        static_cast<double>(graph.Clearances.size()),
        "crossings");
  Notes(into,
        "streets: joint height planes adjusted",
        static_cast<double>(graph.AdjustedJunctions),
        "junctions");
  Notes(into,
        "streets: joint height stations changed",
        static_cast<double>(changedStations),
        "stations");
  Notes(into, "streets: joint height adjustment", plan->MaximumAdjustmentM, "m");
  Notes(into,
        "streets: joint height planning",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
  return {};
}

}
