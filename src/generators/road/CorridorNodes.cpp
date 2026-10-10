#include "Corridors.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {

constexpr double kEndpointWeldM = .25;
constexpr double kEndpointCellM = 2 * kEndpointWeldM;

struct Endpoint {
  uint64_t Node = 0;
  uint64_t Position = 0;
  EastNorth At{};
  int32_t Layer = 0;
  uint8_t TrafficMask = 0;
  bool Owned = false;
  bool Bridge = false;
};

struct Endpoints {
  std::vector<Endpoint> Points;
  std::vector<uint32_t> Parent;
  std::unordered_map<uint64_t, uint32_t> Named;
  std::unordered_map<uint64_t, std::vector<uint32_t>> Cells;
};

uint64_t CellAt(EastNorth at) {
  const auto east = static_cast<int32_t>(std::floor(at.EastM / kEndpointCellM));
  const auto north = static_cast<int32_t>(std::floor(at.NorthM / kEndpointCellM));
  return (static_cast<uint64_t>(static_cast<uint32_t>(east)) << 32u) | static_cast<uint32_t>(north);
}

uint32_t RootOf(uint32_t at, std::vector<uint32_t> &parents) {
  while (parents[at] != at) {
    parents[at] = parents[parents[at]];
    at = parents[at];
  }
  return at;
}

bool Connects(const Endpoint &a, const Endpoint &b) {
  constexpr uint8_t groundTraffic = (1u << static_cast<uint8_t>(Osm::StreetField::Traffic::Road)) |
                                    (1u << static_cast<uint8_t>(Osm::StreetField::Traffic::Path));
  const bool compatible = (a.TrafficMask & b.TrafficMask) != 0;
  const bool groundCrossing = !a.Bridge && !b.Bridge && (a.TrafficMask & groundTraffic) != 0 &&
                              (b.TrafficMask & groundTraffic) != 0;
  const bool sameLevel =
      a.Layer == b.Layer && a.Bridge == b.Bridge && (compatible || groundCrossing);
  const bool transition = a.Position == b.Position && a.Bridge != b.Bridge && a.Owned && b.Owned &&
                          (a.TrafficMask & b.TrafficMask) != 0;
  return (sameLevel || transition) &&
         std::hypot(a.At.EastM - b.At.EastM, a.At.NorthM - b.At.NorthM) <= kEndpointWeldM;
}

bool MergeEndpoint(const Endpoint &point, Endpoint &held) {
  const bool changed =
      (held.TrafficMask | point.TrafficMask) != held.TrafficMask || (point.Owned && !held.Owned);
  held.TrafficMask |= point.TrafficMask;
  held.Owned = held.Owned || point.Owned;
  return changed;
}

void AppendEndpoint(const Endpoint &point, Endpoints &into) {
  const auto [named, added] =
      into.Named.try_emplace(point.Node, static_cast<uint32_t>(into.Points.size()));
  const uint32_t at = named->second;
  if (added) {
    into.Points.push_back(point);
    into.Parent.push_back(at);
  } else if (!MergeEndpoint(point, into.Points[at])) {
    return;
  }
  for (int e = -1; e <= 1; ++e) {
    for (int n = -1; n <= 1; ++n) {
      const auto found = into.Cells.find(CellAt({.EastM = point.At.EastM + e * kEndpointCellM,
                                                 .NorthM = point.At.NorthM + n * kEndpointCellM}));
      if (found == into.Cells.end()) { continue; }
      for (const auto other : found->second) {
        if (other == at || !Connects(into.Points[at], into.Points[other])) { continue; }
        uint32_t first = RootOf(at, into.Parent);
        uint32_t second = RootOf(other, into.Parent);
        if (into.Points[first].Node > into.Points[second].Node) { std::swap(first, second); }
        into.Parent[second] = first;
      }
    }
  }
  if (added) { into.Cells[CellAt(point.At)].push_back(at); }
}

}

uint64_t Corridors::PlannedNodeAt(const Paving &on, uint64_t node) {
  for (;;) {
    const auto found = on.JoinedNodes.find(node);
    if (found == on.JoinedNodes.end() || found->second == node) { return node; }
    node = found->second;
  }
}

void Corridors::JoinCrossing(Crossing &crossing, const Paving &on) {
  const auto &a = on.Ways.Ways()[crossing.Lanes[0]];
  const auto &b = on.Ways.Ways()[crossing.Lanes[1]];
  if (a.Bridge == b.Bridge || a.TrafficKind != b.TrafficKind) { return; }
  const auto points = on.Points;
  for (const auto lane : crossing.Lanes) {
    const auto &way = on.Ways.Ways()[lane];
    if (way.Bridge) { continue; }
    if (way.TrafficKind == Osm::StreetField::Traffic::Road && !way.Ramp) { continue; }
    const auto first = static_cast<size_t>(way.FirstPoint);
    const std::array<size_t, 2> ends{first, first + way.PointCount - 1u};
    for (const auto end : ends) {
      if (!way.OwnsEnds[end == first ? 0 : 1]) { continue; }
      const LongitudeLatitude at{.LongitudeDeg = points[2 * end + 1],
                                 .LatitudeDeg = points[2 * end]};
      const auto local = on.Standing.ToLocalGroundPosition(at);
      if (std::hypot(local.EastM - crossing.EastM, local.NorthM - crossing.NorthM) >
          kEndpointWeldM) {
        continue;
      }
      const std::array nodes{PlannedNodeAt(on, crossing.Nodes[0]),
                             PlannedNodeAt(on, crossing.Nodes[1]),
                             PlannedNodeAt(on, way, at)};
      const uint64_t root = *std::ranges::min_element(nodes);
      for (const auto node : nodes) { on.JoinedNodes.insert_or_assign(node, root); }
    }
  }
}

void Corridors::SettleCrossings(const Paving &on, Paved &into) {
  for (auto &crossing : into.Crossings) {
    for (auto &node : crossing.Nodes) { node = PlannedNodeAt(on, node); }
  }
}

uint64_t Corridors::PlannedNodeAt(const Paving &on,
                                  const Osm::StreetField::Way &lane,
                                  LongitudeLatitude at) {
  return PlannedNodeAt(on, RoadNodeAt(lane, at));
}

std::unordered_map<uint64_t, uint64_t> Corridors::EndpointNodesOf(const Osm::StreetField &ways,
                                                                  const Osm::OsmField &vectors,
                                                                  const TangentFrame &standing) {
  Endpoints endpoints;
  endpoints.Points.reserve(ways.Ways().size() * 2);
  endpoints.Parent.reserve(ways.Ways().size() * 2);
  endpoints.Named.reserve(ways.Ways().size() * 2);
  const auto points = vectors.Points();
  for (const auto &way : ways.Ways()) {
    if (way.Form != Osm::StreetField::Shape::Ribbon || way.PointCount < 2) { continue; }
    const auto first = static_cast<size_t>(way.FirstPoint);
    const std::array<size_t, 2> ends{2u * first, 2u * (first + way.PointCount - 1u)};
    for (const auto end : ends) {
      const LongitudeLatitude at{.LongitudeDeg = points[end + 1], .LatitudeDeg = points[end]};
      const auto local = standing.ToLocalGroundPosition(at);
      AppendEndpoint(
          {.Node = RoadNodeAt(way, at),
           .Position = RoadPositionKey(at),
           .At = {.EastM = local.EastM, .NorthM = local.NorthM},
           .Layer = way.Layer,
           .TrafficMask = static_cast<uint8_t>(1u << std::to_underlying(way.TrafficKind)),
           .Owned = way.OwnsEnds[end == ends.front() ? 0 : 1],
           .Bridge = way.Bridge},
          endpoints);
    }
  }
  std::unordered_map<uint64_t, uint64_t> plan;
  plan.reserve(endpoints.Named.size());
  for (uint32_t at = 0; at < endpoints.Points.size(); ++at) {
    plan.emplace(endpoints.Points[at].Node, endpoints.Points[RootOf(at, endpoints.Parent)].Node);
  }
  return plan;
}

}
