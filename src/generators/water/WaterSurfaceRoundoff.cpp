#include "WaterSurfaceTopology.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <iterator>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Point = std::array<double, 2>;

double Upward(double value) {
  return std::nextafter(value, std::numeric_limits<double>::infinity());
}

bool Adjacent(Point a, Point b) {
  for (size_t axis = 0; axis < a.size(); ++axis) {
    if (!std::isfinite(a[axis]) || !std::isfinite(b[axis]) ||
        (a[axis] != b[axis] && std::nextafter(a[axis], b[axis]) != b[axis])) {
      return false;
    }
  }
  return true;
}

double DistanceBound(Point a, Point b) {
  return Upward(Upward(std::abs(a[0] - b[0])) + Upward(std::abs(a[1] - b[1])));
}

std::vector<uint32_t> Sources(const WaterSurfaceTopology &topology) {
  auto sources = topology.FaceSources;
  std::ranges::sort(sources);
  const auto duplicate = std::ranges::unique(sources);
  sources.erase(duplicate.begin(), duplicate.end());
  return sources;
}

std::vector<uint8_t> Protected(const WaterAsset &water,
                               const TangentFrame &frame,
                               const WaterSurfaceTopology &topology,
                               std::span<const uint32_t> explicitVertices) {
  std::vector<uint8_t> protectedVertices(topology.VerticesXY.size());
  for (const auto vertex : explicitVertices) { protectedVertices[vertex] = 1; }
  std::vector<uint32_t> order(topology.VerticesXY.size());
  std::ranges::iota(order, uint32_t{0});
  const auto coordinates = [&](uint32_t at) { return topology.VerticesXY[at]; };
  std::ranges::sort(order, {}, coordinates);
  for (const auto source : Sources(topology)) {
    const auto rings = water.RingsOf(water.Surfaces()[source]);
    for (const auto &ring : rings.subspan(std::min(size_t{1}, rings.size()))) {
      for (uint32_t at = 0; at < ring.PointCount; ++at) {
        const size_t point = (static_cast<size_t>(ring.FirstPoint) + at) * 2;
        const auto xy = frame.ToLocalGroundPosition(
            {.LongitudeDeg = water.Points()[point + 1], .LatitudeDeg = water.Points()[point]});
        const Point placed{xy.EastM, xy.NorthM};
        const auto found = std::ranges::lower_bound(order, placed, {}, coordinates);
        if (found != order.end() && Adjacent(coordinates(*found), placed)) {
          protectedVertices[*found] = 1;
        }
        if (found != order.begin() && Adjacent(coordinates(*std::prev(found)), placed)) {
          protectedVertices[*std::prev(found)] = 1;
        }
      }
    }
  }
  return protectedVertices;
}

std::vector<std::array<uint32_t, 2>> Candidates(const WaterSurfaceTopology &topology) {
  std::vector<std::array<uint32_t, 2>> edges;
  for (const auto &face : topology.Faces) {
    for (size_t at = 0; at < face.Vertices.size(); ++at) {
      const auto a = face.Vertices[at];
      const auto b = face.Vertices[(at + 1) % face.Vertices.size()];
      if (a != b && Adjacent(topology.VerticesXY[a], topology.VerticesXY[b])) {
        edges.push_back({std::min(a, b), std::max(a, b)});
      }
    }
  }
  std::ranges::sort(edges);
  const auto duplicate = std::ranges::unique(edges);
  edges.erase(duplicate.begin(), duplicate.end());
  return edges;
}

bool AdjacentCluster(const WaterSurfaceTopology &topology,
                     std::span<const uint32_t> mapping,
                     std::array<uint32_t, 2> pair) {
  for (uint32_t at = 0; at < mapping.size(); ++at) {
    if ((mapping[at] == pair[0] || mapping[at] == pair[1]) &&
        !Adjacent(topology.VerticesXY[at], topology.VerticesXY[pair[0]])) {
      return false;
    }
  }
  return true;
}

void RemapCluster(const WaterSurfaceTopology &topology,
                  std::span<uint32_t> mapping,
                  std::array<uint32_t, 2> pair,
                  WaterRoundoffReport &report) {
  for (uint32_t at = 0; at < mapping.size(); ++at) {
    if (mapping[at] == pair[1]) { mapping[at] = pair[0]; }
    if (mapping[at] == pair[0]) {
      report.MaximumDisplacementBoundM =
          std::max(report.MaximumDisplacementBoundM,
                   DistanceBound(topology.VerticesXY[at], topology.VerticesXY[pair[0]]));
    }
  }
}

std::optional<double> Contract(WaterSurfaceTopology &topology, std::array<uint32_t, 2> pair) {
  const auto keep = pair[0];
  const auto remove = pair[1];
  std::vector<WaterSurfaceMesh::Face> faces;
  std::vector<uint32_t> offsets{0};
  std::vector<uint32_t> members;
  double bound = 0;
  double area = 0;
  bool incident = false;
  const auto from = topology.VerticesXY[remove];
  const auto to = topology.VerticesXY[keep];
  const double shift = DistanceBound(from, to);
  for (size_t index = 0; index < topology.Faces.size(); ++index) {
    auto face = topology.Faces[index];
    const bool moved = std::ranges::find(face.Vertices, remove) != face.Vertices.end();
    incident = incident || (moved && std::ranges::find(face.Vertices, keep) != face.Vertices.end());
    if (moved) {
      for (const auto vertex : face.Vertices) {
        if (vertex != remove) {
          const double length = DistanceBound(from, topology.VerticesXY[vertex]);
          bound = Upward(bound + Upward(Upward(shift * length) / 2));
        }
      }
    }
    for (auto &vertex : face.Vertices) {
      if (vertex == remove) { vertex = keep; }
    }
    if (face.Vertices[0] == face.Vertices[1] || face.Vertices[1] == face.Vertices[2] ||
        face.Vertices[2] == face.Vertices[0]) {
      continue;
    }
    const auto &a = topology.VerticesXY[face.Vertices[0]];
    const auto &b = topology.VerticesXY[face.Vertices[1]];
    const auto &c = topology.VerticesXY[face.Vertices[2]];
    const double twice = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (!std::isfinite(twice) || twice <= 0) { return std::nullopt; }
    area += twice / 2;
    faces.push_back(face);
    const auto first = topology.FaceSourceOffsets[index];
    const auto end = topology.FaceSourceOffsets[index + 1];
    const auto source = std::span(topology.FaceSources).subspan(first, end - first);
    members.insert(members.end(), source.begin(), source.end());
    offsets.push_back(static_cast<uint32_t>(members.size()));
  }
  auto sources = members;
  std::ranges::sort(sources);
  const auto duplicate = std::ranges::unique(sources);
  sources.erase(duplicate.begin(), duplicate.end());
  if (!incident || faces.empty() || sources != Sources(topology) || !std::isfinite(bound)) {
    return std::nullopt;
  }
  topology.Faces = std::move(faces);
  topology.FaceSourceOffsets = std::move(offsets);
  topology.FaceSources = std::move(members);
  topology.AreaM2 = area;
  return bound;
}
}

std::expected<WaterRoundoffReport, WaterTopologyError>
ContractWaterRoundoffEdges(const WaterAsset &water,
                           const TangentFrame &frame,
                           WaterSurfaceTopology &topology,
                           WaterRoundoffBudget budget,
                           const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(WaterTopologyError::Canceled); }
  for (const auto vertex : budget.ProtectedVertices) {
    if (vertex >= topology.VerticesXY.size()) {
      return std::unexpected(WaterTopologyError::InvalidInput);
    }
  }
  const auto protectedPoints = Protected(water, frame, topology, budget.ProtectedVertices);
  const auto candidates = Candidates(topology);
  std::vector<uint32_t> mapping(topology.VerticesXY.size());
  std::ranges::iota(mapping, uint32_t{0});
  WaterRoundoffReport report;
  for (const auto edge : candidates) {
    if (stop.stop_requested()) { return std::unexpected(WaterTopologyError::Canceled); }
    const auto keep = std::min(mapping[edge[0]], mapping[edge[1]]);
    const auto remove = std::max(mapping[edge[0]], mapping[edge[1]]);
    if (keep == remove || protectedPoints[keep] != 0 || protectedPoints[remove] != 0) { continue; }
    if (!AdjacentCluster(topology, mapping, {keep, remove})) { continue; }
    if (report.ContractedEdges == budget.Edges) {
      return std::unexpected(WaterTopologyError::CapacityExceeded);
    }
    const auto bound = Contract(topology, {keep, remove});
    if (!bound) { continue; }
    report.SweptAreaBoundM2 = Upward(report.SweptAreaBoundM2 + *bound);
    RemapCluster(topology, mapping, {keep, remove}, report);
    ++report.ContractedEdges;
  }
  return report;
}
}
