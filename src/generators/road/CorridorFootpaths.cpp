#include "Corridors.h"
#include "FootpathConnectionPlan.h"
#include "OsmFootpathSource.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Axes = std::span<const std::vector<RoadStation>>;

bool HasSteps(const Osm::OsmField &field, const Osm::StreetField &ways, Axes axes) {
  for (size_t way = 0; way < axes.size(); ++way) {
    if (axes[way].size() < 2) { continue; }
    const auto source = ways.SourceOf(field, way);
    if (source && field.Str(field.Features()[source->Feature], "kind") == "steps") { return true; }
  }
  return false;
}

std::unordered_set<uint64_t> SharedNodes(Axes axes) {
  std::unordered_set<uint64_t> seen;
  std::unordered_set<uint64_t> shared;
  for (const auto &axis : axes) {
    for (const auto &station : axis) {
      if (station.Node != 0 && !seen.insert(station.Node).second) { shared.insert(station.Node); }
    }
  }
  return shared;
}

struct Inputs {
  std::vector<size_t> Ways;
  std::vector<std::vector<EastNorth>> Axes;
  std::vector<FootpathSource> Sources;
  size_t Refused = 0;
};

FootpathKind Kind(std::string_view kind) {
  if (kind == "steps") { return FootpathKind::Steps; }
  if (kind == "path" || kind == "footway" || kind == "pedestrian") { return FootpathKind::Path; }
  return FootpathKind::Other;
}

Inputs Sources(const Osm::OsmField &field,
               const Osm::StreetField &ways,
               Axes axes,
               const std::unordered_set<uint64_t> &fixed) {
  Inputs input;
  for (size_t way = 0; way < axes.size(); ++way) {
    const auto &axis = axes[way];
    const auto &street = ways.Ways()[way];
    if (axis.size() < 2 || street.TrafficKind != Osm::StreetField::Traffic::Path ||
        street.Form != Osm::StreetField::Shape::Ribbon) {
      continue;
    }
    const auto facts = Osm::ReadFootpathSource(field, ways, way);
    if (!facts) {
      ++input.Refused;
      continue;
    }
    const auto kind = Kind(facts->Kind);
    if (kind == FootpathKind::Other) { continue; }
    std::vector<EastNorth> points;
    points.reserve(axis.size());
    for (const auto &station : axis) {
      points.push_back({.EastM = station.EastM, .NorthM = station.NorthM});
    }
    FootpathSource source{.Axis = points,
                          .HalfWidthM = facts->HalfWidthM,
                          .Kind = kind,
                          .Layer = facts->Placement.Layer.value_or(0),
                          .Bridge = facts->Placement.Bridge.value_or(false),
                          .Tunnel = facts->Placement.Tunnel.value_or(false),
                          .FootAccess = facts->FootAccess.value_or(true),
                          .PositionErrorBoundM = std::nullopt};
    for (size_t end = 0; end < source.Ends.size(); ++end) {
      const auto node = end == 0 ? axis.front().Node : axis.back().Node;
      source.Ends[end] =
          facts->OwnsEnds[end] ? FootpathEndRole::Connectable : FootpathEndRole::Fragment;
      if (fixed.contains(node)) { source.Ends[end] = FootpathEndRole::Fixed; }
    }
    input.Ways.push_back(way);
    input.Axes.push_back(std::move(points));
    input.Sources.push_back(source);
  }
  return input;
}

std::vector<RoadStation> ConnectedAxis(uint32_t source,
                                       const Inputs &input,
                                       const FootpathConnectionPlan &plan,
                                       Axes original,
                                       std::span<const RoadStation> joints) {
  const auto &old = original[input.Ways[source]];
  const auto vertices = plan.Axis(source, input.Axes[source]);
  std::array<bool, 2> extended{};
  for (const auto &edit : plan.EditsOf(source)) {
    const auto &connection = plan.Connections()[edit.Connection];
    if (connection.Branch == source && connection.AdvanceM > 0) { extended[connection.End] = true; }
  }
  std::vector<RoadStation> result;
  result.reserve(vertices.size());
  for (const auto &vertex : vertices) {
    RoadStation station;
    if (vertex.Original != FootpathAxisVertex::kNoIndex) {
      station = old[vertex.Original];
      if ((vertex.Original == 0 && extended[0]) ||
          (vertex.Original + 1 == old.size() && extended[1])) {
        station.Node = 0;
      }
    }
    if (vertex.Connection != FootpathAxisVertex::kNoIndex) {
      station.GradeM = joints[vertex.Connection].GradeM;
      station.Node = joints[vertex.Connection].Node;
    }
    station.EastM = vertex.At.EastM;
    station.NorthM = vertex.At.NorthM;
    result.push_back(station);
  }
  return result;
}
}

void Corridors::ConnectFootpaths(const Paving &on, Paved &into) {
  if (!HasSteps(on.Vectors, on.Ways, into.Designed)) { return; }
  auto fixed = SharedNodes(into.Designed);
  for (const auto &[node, count] : on.SharedNodes) {
    if (count > 1) { fixed.insert(PlannedNodeAt(on, node)); }
  }
  for (const auto &[node, root] : on.JoinedNodes) {
    if (node != root) { fixed.insert(PlannedNodeAt(on, root)); }
  }
  const auto input = Sources(on.Vectors, on.Ways, into.Designed, fixed);
  Notes(into, "footpaths: refused sources", static_cast<double>(input.Refused), "ways");
  const auto plan = FootpathConnectionPlan::Prepare(input.Sources);
  if (!plan) {
    Notes(into, "footpaths: refused plan", static_cast<double>(plan.error()), "reason");
    return;
  }
  std::vector<RoadStation> joints;
  joints.reserve(plan->Connections().size());
  for (const auto &connection : plan->Connections()) {
    const size_t target = input.Ways[connection.Target];
    const auto &a = into.Designed[target][connection.Segment];
    const auto &b = into.Designed[target][connection.Segment + 1];
    const auto at = connection.JoinedAt;
    uint64_t node = 0;
    if (connection.TargetFraction == 0) { node = a.Node; }
    if (connection.TargetFraction == 1) { node = b.Node; }
    if (node == 0) {
      node = PlannedNodeAt(on, on.Ways.Ways()[target], on.Standing.ApproximateGeographicAt(at));
    }
    joints.push_back({.EastM = at.EastM,
                      .NorthM = at.NorthM,
                      .GradeM = std::lerp(a.GradeM, b.GradeM, connection.TargetFraction),
                      .Node = node});
  }
  std::vector<std::pair<size_t, std::vector<RoadStation>>> updates;
  for (uint32_t source = 0; source < input.Sources.size(); ++source) {
    if (!plan->EditsOf(source).empty()) {
      updates.emplace_back(input.Ways[source],
                           ConnectedAxis(source, input, *plan, into.Designed, joints));
    }
  }
  for (auto &[way, axis] : updates) { into.Designed[way] = std::move(axis); }
  Notes(into, "footpaths: joined ends", static_cast<double>(joints.size()), "ends");
  Notes(into,
        "footpaths: source segments",
        static_cast<double>(plan->Work().SourceSegments),
        "segments");
  Notes(into,
        "footpaths: index build containers",
        static_cast<double>(plan->Work().IndexBuildHeapBytes),
        "bytes");
  Notes(into,
        "footpaths: candidate tests",
        static_cast<double>(plan->Work().CandidateTests),
        "tests");
  Notes(
      into, "footpaths: budget refusals", static_cast<double>(plan->Work().BudgetRefusals), "ends");
}

}
