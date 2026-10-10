#include "Corridors.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
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
  const bool sameLevel = a.Layer == b.Layer && a.Bridge == b.Bridge;
  const bool transition = a.Position == b.Position && a.Bridge != b.Bridge;
  return (sameLevel || transition) &&
         std::hypot(a.At.EastM - b.At.EastM, a.At.NorthM - b.At.NorthM) <= kEndpointWeldM;
}

void AppendEndpoint(const Endpoint &point, Endpoints &into) {
  const auto at = static_cast<uint32_t>(into.Points.size());
  if (!into.Named.try_emplace(point.Node, at).second) { return; }
  into.Points.push_back(point);
  into.Parent.push_back(at);
  for (int e = -1; e <= 1; ++e) {
    for (int n = -1; n <= 1; ++n) {
      const auto found = into.Cells.find(CellAt({.EastM = point.At.EastM + e * kEndpointCellM,
                                                 .NorthM = point.At.NorthM + n * kEndpointCellM}));
      if (found == into.Cells.end()) { continue; }
      for (const auto other : found->second) {
        if (!Connects(point, into.Points[other])) { continue; }
        uint32_t first = RootOf(at, into.Parent);
        uint32_t second = RootOf(other, into.Parent);
        if (into.Points[first].Node > into.Points[second].Node) { std::swap(first, second); }
        into.Parent[second] = first;
      }
    }
  }
  into.Cells[CellAt(point.At)].push_back(at);
}

}

uint64_t Corridors::PlannedNodeAt(const Paving &on,
                                  const Osm::StreetField::Way &lane,
                                  LongitudeLatitude at) {
  const uint64_t node = RoadNodeAt(lane, at);
  const auto found = on.EndNodes.find(node);
  return found == on.EndNodes.end() ? node : found->second;
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
      AppendEndpoint({.Node = RoadNodeAt(way, at),
                      .Position = RoadPositionKey(at),
                      .At = {.EastM = local.EastM, .NorthM = local.NorthM},
                      .Layer = way.Layer,
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
