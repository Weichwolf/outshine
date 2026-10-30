#ifndef OUTSHINE_TEST_CANONICALGROUNDDEMANDFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDDEMANDFIXTURE_H
#include "CanonicalGroundSourceFixture.h"
#include "Check.h"
#include "GeoCellId.h"
#include "TransportCoreSelection.h"
#include "TransportNetworkSnapshot.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Test::CanonicalGroundFixture {
struct Demand {
  World::TransportEdgeId Edge;
  Generators::TransportBoundary Boundary;
  LongitudeLatitude Anchor;
  std::array<LongitudeLatitude, 2> OriginalSegment;
};

using DemandKey =
    std::tuple<World::TransportEdgeId, int, Generators::TransportBoundaryAxis, uint32_t, uint32_t>;
inline constexpr int kDemandLevel = 20;
inline constexpr uint32_t kDemandCells = uint32_t{1} << kDemandLevel;

[[nodiscard]] inline auto Cell(const World::TransportNode &node) {
  return std::pair{static_cast<uint32_t>(std::floor((node.LongitudeDeg + kDegPerHalfTurn) /
                                                    (2 * kDegPerHalfTurn) * kDemandCells)),
                   static_cast<uint32_t>(std::floor((node.LatitudeDeg + kDegPerHalfTurn / 2) /
                                                    kDegPerHalfTurn * kDemandCells))};
}

[[nodiscard]] inline auto Cells(const Source &input,
                                std::span<const World::TransportEdgeId> seeds) {
  constexpr size_t kCellsMost = 1024;
  std::set<std::pair<uint32_t, uint32_t>> cells;
  const auto &graph = input.Snapshot->Topology();
  for (const auto seed : seeds) {
    const auto *edge = graph.FindEdge(seed);
    CHECK(edge != nullptr, "each measured port demand belongs to an actual source edge");
    if (edge == nullptr) { return cells; }
    const auto a = Cell(*graph.FindNode(edge->FromNodeId));
    const auto b = Cell(*graph.FindNode(edge->ToNodeId));
    for (uint32_t x = std::min(a.first, b.first); x <= std::max(a.first, b.first); ++x) {
      for (uint32_t y = std::min(a.second, b.second); y <= std::max(a.second, b.second); ++y) {
        if (cells.size() >= kCellsMost) {
          CHECK(false, "native request enumeration remains bounded");
          return cells;
        }
        cells.emplace(x, y);
      }
    }
  }
  return cells;
}

inline void CollectDemands(const World::TransportTopology &graph,
                           const Generators::TransportCoreSegment &segment,
                           std::map<DemandKey, Demand> &unique) {
  const auto &edge = graph.Edges()[segment.SourceEdge];
  auto id = edge.Id;
  id.Direction = World::EdgeDirection::Forward;
  const auto *from = graph.FindNode(edge.FromNodeId);
  const auto *to = graph.FindNode(edge.ToNodeId);
  std::array<LongitudeLatitude, 2> original{
      {{.LongitudeDeg = from->LongitudeDeg, .LatitudeDeg = from->LatitudeDeg},
       {.LongitudeDeg = to->LongitudeDeg, .LatitudeDeg = to->LatitudeDeg}}};
  if (edge.Id.Direction == World::EdgeDirection::Reverse) { std::ranges::reverse(original); }
  for (const auto &end : segment.Ends) {
    if (!std::isfinite(end.Fraction) || end.Fraction <= 0 || end.Fraction >= 1) { continue; }
    for (const auto &boundary : end.Boundaries) {
      if (!boundary) { continue; }
      const DemandKey key{id, boundary->Level, boundary->Axis, boundary->Line, boundary->Segment};
      unique.try_emplace(key,
                         Demand{.Edge = id,
                                .Boundary = *boundary,
                                .Anchor = end.Geographic,
                                .OriginalSegment = original});
    }
  }
}

[[nodiscard]] inline std::vector<Demand> Demands(const Source &input,
                                                 std::span<const World::TransportEdgeId> seeds) {
  const auto &graph = input.Snapshot->Topology();
  std::set<World::TransportEdgeId> wanted;
  for (auto seed : seeds) {
    seed.Direction = World::EdgeDirection::Forward;
    wanted.insert(seed);
  }
  std::map<DemandKey, Demand> unique;
  const Generators::TransportCoreLimits limits{.SourceNodesMost = graph.Nodes().size(),
                                               .SourceEdgesMost = graph.Edges().size(),
                                               .SegmentsMost = 2048,
                                               .ContainerCapacityMost = size_t{1024} * 1024};
  for (const auto [x, y] : Cells(input, seeds)) {
    const auto selected = Generators::SelectTransportCore(
        {.Level = kDemandLevel, .X = x, .Y = y}, input.Snapshot, limits);
    CHECK(selected.has_value(),
          "actual geographic cores use the existing native clipping consumer");
    if (!selected) { return {}; }
    for (const auto &segment : selected->Segments) {
      auto id = graph.Edges()[segment.SourceEdge].Id;
      id.Direction = World::EdgeDirection::Forward;
      if (wanted.contains(id)) { CollectDemands(graph, segment, unique); }
    }
  }
  std::vector<Demand> result;
  for (const auto &[key, demand] : unique) {
    (void)key;
    result.push_back(demand);
  }
  return result;
}
}
#endif
