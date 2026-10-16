#include "Corridors.h"
#include "RoadHeightPlan.h"
#include "RoadCrossSection.h"
#include "JunctionSlopePlan.h"
#include "Heap.h"
#include "Log.h"

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
#include <unordered_set>
#include <vector>

namespace outshine::Generators {
namespace {

struct HeightAttachment {
  uint64_t Node = 0;
  double OffsetM = 0.0;
  uint32_t Junction = std::numeric_limits<uint32_t>::max();
};

enum class CrossingCoreRecipe { Flat, Profiled };

struct ProfileLimits {
  double MaxGradient = 0.0;
  double MinimumM = 0.0;
  CrossingCoreRecipe Core = CrossingCoreRecipe::Flat;
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
  std::vector<std::array<size_t, 2>> ClearanceLanes;
  std::vector<uint32_t> Stations;
  std::vector<double> OffsetsM;
  std::vector<uint32_t> StationPlanes;
  std::vector<uint32_t> LinkStations;
  std::vector<JunctionSlopeConstraint> PlaneConstraints;
  std::unordered_map<uint64_t, uint32_t> Named;
  std::unordered_map<uint64_t, uint64_t> HeightOwners;
  std::unordered_map<uint64_t, uint32_t> JunctionAt;
  std::unordered_map<uint64_t, double> CrossingCoreRadiusM;
  std::unordered_map<size_t, double> RigidRiseM;
  std::unordered_set<size_t> FlexibleCoreLinks;
  std::vector<bool> Adjusted;
  size_t AdjustedJunctions = 0;
};

namespace {

constexpr double kStationChangeToleranceM = .01;
constexpr double kPlaneClearanceMarginM = 1e-8;
constexpr size_t kPlaneAttemptsMost = 64;
constexpr auto kPlaneTimeBudget = std::chrono::seconds(5);
constexpr size_t kCycleLogConstraintsMost = 16;

uint64_t ProfileNode(std::span<const RoadStation> along, std::array<uint64_t, 2> ends, size_t at) {
  if (at == 0) { return ends[0]; }
  if (at + 1 == along.size()) { return ends[1]; }
  return along[at].Node;
}

std::unordered_map<uint64_t, Vec2> CrossingDirections(const auto &into, const auto &graph) {
  std::unordered_map<uint64_t, Vec2> directions;
  for (const auto &edge : into.Edges) {
    const auto along = std::span(into.Planned).subspan(edge.PlannedFirst, edge.PlannedCount);
    if (along.size() < 2) { continue; }
    for (size_t at = 0; at < along.size(); ++at) {
      const uint64_t node = ProfileNode(along, edge.NodeAt, at);
      if (!graph.CrossingCoreRadiusM.contains(node)) { continue; }
      const auto &first = along[at == 0 ? 0 : at - 1];
      const auto &last = along[at + 1 == along.size() ? at : at + 1];
      const double east = last.EastM - first.EastM;
      const double north = last.NorthM - first.NorthM;
      const double length = std::hypot(east, north);
      if (length > 0) { directions.try_emplace(node, Vec2{{east / length, north / length}}); }
    }
  }
  return directions;
}

bool SeparatedCrossing(const auto &on, const auto &crossing) {
  if (crossing.Nodes[0] == 0 || crossing.Nodes[1] == 0 || crossing.Nodes[0] == crossing.Nodes[1]) {
    return false;
  }
  const auto &first = on.Ways.Ways()[crossing.Lanes[0]];
  const auto &second = on.Ways.Ways()[crossing.Lanes[1]];
  return (first.Bridge || second.Bridge) &&
         (first.Layer != second.Layer || first.Bridge != second.Bridge);
}

void PrepareCrossingCores(const auto &on, const auto &into, auto &graph, const auto &halfWidth) {
  graph.CrossingCoreRadiusM.clear();
  for (const auto &crossing : into.Crossings) {
    if (!SeparatedCrossing(on, crossing)) { continue; }
    for (const uint64_t node : crossing.Nodes) { graph.CrossingCoreRadiusM.try_emplace(node, 0); }
  }
  const auto directions = CrossingDirections(into, graph);
  for (const auto &crossing : into.Crossings) {
    if (!SeparatedCrossing(on, crossing)) { continue; }
    const auto first = directions.find(crossing.Nodes[0]);
    const auto second = directions.find(crossing.Nodes[1]);
    if (first == directions.end() || second == directions.end()) { continue; }
    const double sine =
        std::abs(first->second[0] * second->second[1] - first->second[1] * second->second[0]);
    if (!(sine > 0)) { continue; }
    const double radius = (halfWidth(crossing.Lanes[0]) + halfWidth(crossing.Lanes[1])) / sine;
    if (!std::isfinite(radius)) { continue; }
    for (const uint64_t node : crossing.Nodes) {
      graph.CrossingCoreRadiusM[node] = std::max(graph.CrossingCoreRadiusM[node], radius);
    }
  }
}

std::vector<std::array<double, 2>>
CrossingCoreIntervals(std::span<const RoadStation> along,
                      const std::array<HeightAttachment, 2> &ends,
                      const auto &graph) {
  std::vector<std::array<double, 2>> intervals;
  double reached = 0;
  for (size_t at = 0; at < along.size(); ++at) {
    if (at > 0) {
      reached += std::hypot(along[at].EastM - along[at - 1].EastM,
                            along[at].NorthM - along[at - 1].NorthM);
    }
    const uint64_t node = ProfileNode(along, {ends[0].Node, ends[1].Node}, at);
    const auto found = graph.CrossingCoreRadiusM.find(node);
    if (found != graph.CrossingCoreRadiusM.end() && found->second > 0) {
      intervals.push_back({reached - found->second, reached + found->second});
    }
  }
  std::ranges::sort(intervals);
  std::vector<std::array<double, 2>> merged;
  for (const auto &interval : intervals) {
    if (merged.empty() || interval[0] > merged.back()[1]) {
      merged.push_back(interval);
    } else {
      merged.back()[1] = std::max(merged.back()[1], interval[1]);
    }
  }
  return merged;
}

bool SelectFlexibleCores(const RoadHeightFailure &failure, auto &graph) {
  bool changed = false;
  for (const auto &constraint : failure.CycleConstraints) {
    if (constraint.Kind == RoadHeightConstraintKind::Clearance) { continue; }
    const auto found = graph.RigidRiseM.find(constraint.Index);
    if (found == graph.RigidRiseM.end() || !(found->second > 0)) { continue; }
    changed |= graph.FlexibleCoreLinks.insert(constraint.Index).second;
  }
  return changed;
}

bool SelectLevelDecks(const RoadHeightFailure &failure,
                      const auto &on,
                      auto &into,
                      const auto &graph) {
  bool changed = false;
  for (const auto &constraint : failure.CycleConstraints) {
    if (constraint.Kind != RoadHeightConstraintKind::Clearance) { continue; }
    for (const size_t lane : graph.ClearanceLanes[constraint.Index]) {
      if (on.Ways.Ways()[lane].Bridge) { changed |= into.UnbankedWays.insert(lane).second; }
    }
  }
  return changed;
}

void LogHeightCycle(const RoadHeightFailure &failure,
                    const auto &on,
                    const auto &into,
                    const auto &graph) {
  const auto cycle =
      std::span(failure.CycleConstraints)
          .first(std::min(failure.CycleConstraints.size(), kCycleLogConstraintsMost));
  for (const auto &constraint : cycle) {
    if (constraint.Kind == RoadHeightConstraintKind::Clearance) {
      const auto &gap = graph.Clearances[constraint.Index];
      Log::Error(LogTag::World,
                 "road_cycle_gap",
                 {{"lower", static_cast<int64_t>(gap.Lower)},
                  {"upper", static_cast<int64_t>(gap.Upper)},
                  {"gapM", gap.MinimumGapM}});
      continue;
    }
    const auto &link = graph.Links[constraint.Index];
    const size_t at = graph.LinkStations[constraint.Index];
    const auto edge = std::ranges::find_if(into.Edges, [&](const auto &one) {
      return one.PlannedFirst <= at && at + 1 < one.PlannedFirst + one.PlannedCount;
    });
    if (edge == into.Edges.end()) { continue; }
    const auto &way = on.Ways.Ways()[edge->Lane];
    Log::Error(LogTag::World,
               "road_cycle_link",
               {{"lane", static_cast<int64_t>(edge->Lane)},
                {"first", static_cast<int64_t>(link.First)},
                {"second", static_cast<int64_t>(link.Second)},
                {"riseM", link.MaximumRiseM},
                {"offset0", link.FirstOffsetM},
                {"offset1", link.SecondOffsetM},
                {"layer", way.Layer},
                {"bridge", way.Bridge},
                {"halfM", way.HalfWidthM}});
  }
}

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
  const auto cores = CrossingCoreIntervals(along, ends, graph);
  size_t core = 0;
  double reached = 0;
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
      while (core < cores.size() && cores[core][1] <= reached) { ++core; }
      const bool rigid = limits.Core == CrossingCoreRecipe::Flat && core < cores.size() &&
                         cores[core][0] < reached + runM &&
                         !graph.FlexibleCoreLinks.contains(graph.Links.size());
      if (rigid) { graph.RigidRiseM.emplace(graph.Links.size(), limits.MaxGradient * runM); }
      graph.Links.push_back({previous,
                             node,
                             rigid ? 0 : limits.MaxGradient * runM,
                             previousOffsetM,
                             attachment.OffsetM});
      graph.LinkStations.push_back(static_cast<uint32_t>(graph.Stations.size() - 2));
      reached += runM;
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
    const auto &constraint = graph.PlaneConstraints.back();
    Log::Error(LogTag::World,
               "road_height_cycle",
               {{"constraints", static_cast<int64_t>(graph.PlaneConstraints.size())},
                {"cycle", static_cast<int64_t>(failure.CycleConstraints.size())},
                {"planes", static_cast<int64_t>(constraint.Terms.size())},
                {"riseM", constraint.MinimumRiseM}});
    return std::unexpected(plan.error().Reason());
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
    const auto bankExtent = [&](size_t side) {
      const size_t lane = crossing.Lanes[side];
      const auto &way = on.Ways.Ways()[lane];
      if (!way.Bridge || into.UnbankedWays.contains(lane)) { return 0.0; }
      const Section section = RoadSection(way.HalfWidthM, ProfileOf(on, lane));
      return (section.HalfWidthM + section.ShoulderM) * kCrossfall;
    };
    const double lowerBankM = bankExtent(lower);
    const double upperBankM = bankExtent(upper);
    if (lowerWay.Bridge) {
      auto &node = graph.Nodes[below->second];
      node.MinimumM = std::max(node.MinimumM, crossing.GradeM + kSealedDepthM + lowerBankM);
    }
    graph.Clearances.push_back({.Lower = below->second,
                                .Upper = above->second,
                                .MinimumGapM = lowerWay.ClearanceM +
                                               (upperWay.Bridge ? kSealedDepthM : 0) + lowerBankM +
                                               upperBankM});
    graph.ClearanceLanes.push_back({crossing.Lanes[lower], crossing.Lanes[upper]});
  }
}

void Corridors::HeightConstraints(const Paving &on, Paved &into, HeightGraph &graph) {
  PrepareCrossingCores(on, into, graph, [&](size_t lane) {
    const Section section = RoadSection(on.Ways.Ways()[lane].HalfWidthM, ProfileOf(on, lane));
    return section.HalfWidthM + section.ShoulderM;
  });
  graph.Nodes.clear();
  graph.Links.clear();
  graph.RigidRiseM.clear();
  graph.Clearances.clear();
  graph.ClearanceLanes.clear();
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
    const bool isolated = std::ranges::all_of(
        ends, [](const auto &end) { return end.Junction == std::numeric_limits<uint32_t>::max(); });
    if (!isolated && !CrossingCoreIntervals(along, ends, graph).empty()) {
      into.UnbankedWays.insert(edge.Lane);
    }
    const double minimumM = way.Bridge && into.DeckM[edge.Lane] > kUnraisedDeckM
                                ? into.DeckM[edge.Lane]
                                : -std::numeric_limits<double>::infinity();
    AppendProfile(along,
                  ends,
                  {.MaxGradient = RoadGradient(way),
                   .MinimumM = minimumM,
                   .Core = isolated ? CrossingCoreRecipe::Flat : CrossingCoreRecipe::Profiled},
                  graph);
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
    if (SelectFlexibleCores(failure, graph) || SelectLevelDecks(failure, on, into, graph)) {
      graph.PlaneConstraints.clear();
    } else {
      if (auto adjusted = AdjustJunctionPlanes(failure, into, graph, began + kPlaneTimeBudget);
          !adjusted) {
        LogHeightCycle(failure, on, into, graph);
        return adjusted;
      }
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
    BindJunctionPorts(junction, into);
  }
  Notes(into,
        "streets: flexible crossing core links",
        static_cast<double>(graph.FlexibleCoreLinks.size()),
        "links");
  Notes(
      into, "streets: level crossing decks", static_cast<double>(into.UnbankedWays.size()), "ways");
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
