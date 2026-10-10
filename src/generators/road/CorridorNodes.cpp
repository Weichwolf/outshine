#include "Corridors.h"
#include "JunctionFootprint.h"

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

EastNorth WayPoint(const auto &on, size_t at) {
  return on.Standing.ToLocalGroundPosition(
      {.LongitudeDeg = on.Points[2 * at + 1], .LatitudeDeg = on.Points[2 * at]});
}

bool WithinLanding(const auto &on,
                   const Osm::StreetField::Way &way,
                   size_t end,
                   EastNorth crossing) {
  double reachedM = 0.0;
  size_t at = static_cast<size_t>(way.FirstPoint) + (end ? way.PointCount - 1u : 0u);
  EastNorth first = WayPoint(on, at);
  for (uint32_t step = 1; step < way.PointCount; ++step) {
    at = end ? at - 1u : at + 1u;
    const EastNorth second = WayPoint(on, at);
    const double dE = second.EastM - first.EastM;
    const double dN = second.NorthM - first.NorthM;
    const double squared = dE * dE + dN * dN;
    if (!(squared > 0)) { continue; }
    const double fraction = std::clamp(
        ((crossing.EastM - first.EastM) * dE + (crossing.NorthM - first.NorthM) * dN) / squared,
        0.0,
        1.0);
    const double apart = std::hypot(crossing.EastM - first.EastM - fraction * dE,
                                    crossing.NorthM - first.NorthM - fraction * dN);
    const double spanM = std::sqrt(squared);
    if (apart <= kEndpointWeldM && reachedM + fraction * spanM <= kJunctionCoreReachM) {
      return true;
    }
    reachedM += spanM;
    if (reachedM > kJunctionCoreReachM) { return false; }
    first = second;
  }
  return false;
}

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

bool Corridors::JoinLanding(const Crossing &crossing, const Paving &on) {
  const auto &a = on.Ways.Ways()[crossing.Lanes[0]];
  const auto &b = on.Ways.Ways()[crossing.Lanes[1]];
  const auto points = on.Points;
  for (size_t firstEnd = 0; firstEnd < 2; ++firstEnd) {
    if (!a.OwnsEnds[firstEnd]) { continue; }
    const size_t firstPoint = a.FirstPoint + (firstEnd != 0 ? a.PointCount - 1u : 0u);
    const LongitudeLatitude firstAt{.LongitudeDeg = points[2 * firstPoint + 1],
                                    .LatitudeDeg = points[2 * firstPoint]};
    const uint64_t port = PlannedNodeAt(on, a, firstAt);
    for (size_t secondEnd = 0; secondEnd < 2; ++secondEnd) {
      if (!b.OwnsEnds[secondEnd]) { continue; }
      const size_t secondPoint = b.FirstPoint + (secondEnd != 0 ? b.PointCount - 1u : 0u);
      const LongitudeLatitude secondAt{.LongitudeDeg = points[2 * secondPoint + 1],
                                       .LatitudeDeg = points[2 * secondPoint]};
      if (port != PlannedNodeAt(on, b, secondAt)) { continue; }
      const EastNorth at{.EastM = crossing.EastM, .NorthM = crossing.NorthM};
      if (!WithinLanding(on, a, firstEnd, at) || !WithinLanding(on, b, secondEnd, at)) { continue; }
      const std::array nodes{
          port, PlannedNodeAt(on, crossing.Nodes[0]), PlannedNodeAt(on, crossing.Nodes[1])};
      const uint64_t root = *std::ranges::min_element(nodes);
      for (const auto node : nodes) { on.JoinedNodes.insert_or_assign(node, root); }
      return true;
    }
  }
  return false;
}

void Corridors::JoinCrossing(Crossing &crossing, const Paving &on) {
  const auto &a = on.Ways.Ways()[crossing.Lanes[0]];
  const auto &b = on.Ways.Ways()[crossing.Lanes[1]];
  if (a.Bridge == b.Bridge || a.TrafficKind != b.TrafficKind || JoinLanding(crossing, on)) {
    return;
  }
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
