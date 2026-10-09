#include "Corridors.h"
#include "RoadHeightPlan.h"
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
};

struct ProfileLimits {
  double MaxGradient = 0.0;
  double MinimumM = 0.0;
};

}

struct Corridors::HeightGraph {
  std::vector<RoadHeightNode> Nodes;
  std::vector<RoadHeightLink> Links;
  std::vector<uint32_t> Stations;
  std::vector<double> OffsetsM;
  std::unordered_map<uint64_t, uint32_t> Named;
  std::unordered_map<uint64_t, uint32_t> JunctionAt;
  std::vector<bool> Adjusted;
  size_t AdjustedJunctions = 0;
};

namespace {

constexpr double kStationChangeToleranceM = .01;
constexpr double kPlaneScaleMargin = 1e-6;
constexpr size_t kPlaneAttemptsMost = 64;
constexpr auto kPlaneTimeBudget = std::chrono::seconds(5);

uint32_t
HeightNode(const RoadStation &station, HeightAttachment attachment, double minimumM, auto &graph) {
  auto node = static_cast<uint32_t>(graph.Nodes.size());
  if (attachment.Node != 0) {
    const auto [found, inserted] = graph.Named.try_emplace(attachment.Node, node);
    if (!inserted) { node = found->second; }
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
    if (at > 0) {
      const double runM = std::hypot(along[at].EastM - along[at - 1].EastM,
                                     along[at].NorthM - along[at - 1].NorthM);
      graph.Links.push_back(
          {previous, node, limits.MaxGradient * runM, previousOffsetM, attachment.OffsetM});
    }
    previous = node;
    previousOffsetM = attachment.OffsetM;
  }
}

bool AdjustJunctionPlanes(const RoadHeightFailure &failure, auto &into, auto &graph) {
  bool changed = false;
  for (const uint32_t node : failure.CycleNodes) {
    if (node >= into.Junctions.size()) { continue; }
    auto &junction = into.Junctions[node];
    if (junction.SlopeE == 0 && junction.SlopeN == 0) { continue; }
    const double scale = failure.MaximumOffsetScale * (1.0 - kPlaneScaleMargin);
    junction.SlopeE *= scale;
    junction.SlopeN *= scale;
    if (!graph.Adjusted[node]) { ++graph.AdjustedJunctions; }
    graph.Adjusted[node] = true;
    changed = true;
  }
  return changed;
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

void Corridors::HeightConstraints(const Paving &on, Paved &into, HeightGraph &graph) {
  graph.Nodes.clear();
  graph.Links.clear();
  graph.Stations.clear();
  graph.OffsetsM.clear();
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
  graph.Stations.reserve(into.Planned.size());
  graph.OffsetsM.reserve(into.Planned.size());
  graph.Named.reserve(into.Edges.size() * 2);
  graph.JunctionAt.reserve(into.Junctions.size());
  for (uint32_t at = 0; at < into.Junctions.size(); ++at) {
    graph.JunctionAt.emplace(into.Junctions[at].Node, at);
  }
  graph.Adjusted.resize(into.Junctions.size());
  HeightConstraints(on, into, graph);
  auto plan = PlanRoadHeights(graph.Nodes, graph.Links, RoadHeightFit::PreferCuts);
  for (size_t attempt = 0; !plan; ++attempt) {
    const auto &failure = plan.error();
    if (failure.CycleNodes.empty()) { return std::unexpected(failure.Reason); }
    if (attempt >= kPlaneAttemptsMost ||
        std::chrono::steady_clock::now() - began >= kPlaneTimeBudget) {
      return std::unexpected("road junction plane planning exceeded its work bound");
    }
    if (!AdjustJunctionPlanes(failure, into, graph)) { return std::unexpected(failure.Reason); }
    HeightConstraints(on, into, graph);
    plan = PlanRoadHeights(graph.Nodes, graph.Links, RoadHeightFit::PreferCuts);
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
