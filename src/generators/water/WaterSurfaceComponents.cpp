#include "WaterSurfaceTopology.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <numeric>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = WaterTopologyError;

uint32_t Root(std::span<uint32_t> parents, uint32_t at) {
  while (parents[at] != at) {
    parents[at] = parents[parents[at]];
    at = parents[at];
  }
  return at;
}

std::expected<std::vector<uint32_t>, Error> FaceComponents(const WaterSurfaceTopology &topology) {
  std::vector<std::array<uint32_t, 3>> edges;
  edges.reserve(topology.Faces.size() * 3);
  for (uint32_t face = 0; face < topology.Faces.size(); ++face) {
    const auto &indices = topology.Faces[face].Vertices;
    for (size_t at = 0; at < indices.size(); ++at) {
      const auto a = indices[at];
      const auto b = indices[(at + 1) % indices.size()];
      if (a >= topology.VerticesXY.size() || b >= topology.VerticesXY.size() || a == b) {
        return std::unexpected(Error::InvalidGeometry);
      }
      edges.push_back({std::min(a, b), std::max(a, b), face});
    }
  }
  std::ranges::sort(edges);
  std::vector<uint32_t> parents(topology.Faces.size());
  std::ranges::iota(parents, uint32_t{0});
  size_t first = 0;
  while (first < edges.size()) {
    size_t end = first + 1;
    while (end < edges.size() && edges[end][0] == edges[first][0] &&
           edges[end][1] == edges[first][1]) {
      ++end;
    }
    if (end - first > 2) { return std::unexpected(Error::InvalidGeometry); }
    if (end - first == 2) {
      const auto a = Root(parents, edges[first][2]);
      const auto b = Root(parents, edges[first + 1][2]);
      parents[std::max(a, b)] = std::min(a, b);
    }
    first = end;
  }
  for (uint32_t at = 0; at < parents.size(); ++at) { parents[at] = Root(parents, at); }
  return parents;
}

std::expected<WaterSurfaceTopology, Error> ExtractComponent(const WaterSurfaceTopology &topology,
                                                            std::span<const uint32_t> faces) {
  WaterSurfaceTopology part;
  part.Anchor = topology.Anchor;
  std::vector<uint32_t> vertices;
  vertices.reserve(faces.size() * 3);
  for (const auto face : faces) {
    const auto &triangle = topology.Faces[face].Vertices;
    vertices.insert(vertices.end(), triangle.begin(), triangle.end());
  }
  std::ranges::sort(vertices);
  const auto duplicates = std::ranges::unique(vertices);
  vertices.erase(duplicates.begin(), duplicates.end());
  part.VerticesXY.reserve(vertices.size());
  for (const auto vertex : vertices) { part.VerticesXY.push_back(topology.VerticesXY[vertex]); }
  part.FaceSourceOffsets.push_back(0);
  for (const auto index : faces) {
    auto face = topology.Faces[index];
    for (auto &vertex : face.Vertices) {
      vertex = static_cast<uint32_t>(std::ranges::lower_bound(vertices, vertex) - vertices.begin());
    }
    const auto &a = part.VerticesXY[face.Vertices[0]];
    const auto &b = part.VerticesXY[face.Vertices[1]];
    const auto &c = part.VerticesXY[face.Vertices[2]];
    const double twice = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (!std::isfinite(twice) || !(twice > 0)) { return std::unexpected(Error::InvalidGeometry); }
    part.AreaM2 += twice / 2;
    part.Faces.push_back(face);
    const auto first = topology.FaceSourceOffsets[index];
    const auto end = topology.FaceSourceOffsets[index + 1];
    if (first >= end || end > topology.FaceSources.size()) {
      return std::unexpected(Error::InvalidInput);
    }
    const auto sources = std::span(topology.FaceSources).subspan(first, end - first);
    part.FaceSources.insert(part.FaceSources.end(), sources.begin(), sources.end());
    part.FaceSourceOffsets.push_back(static_cast<uint32_t>(part.FaceSources.size()));
  }
  return part;
}
}

std::expected<std::vector<WaterSurfaceTopology>, WaterTopologyError>
SplitWaterSurfaceComponents(WaterSurfaceTopology topology, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (topology.Faces.empty() || topology.Faces.size() > std::numeric_limits<uint32_t>::max() / 3 ||
      topology.FaceSourceOffsets.size() != topology.Faces.size() + 1 ||
      topology.FaceSourceOffsets.front() != 0 ||
      topology.FaceSourceOffsets.back() != topology.FaceSources.size()) {
    return std::unexpected(Error::InvalidInput);
  }
  const auto parents = FaceComponents(topology);
  if (!parents) { return std::unexpected(parents.error()); }
  std::map<uint32_t, std::vector<uint32_t>> groups;
  for (uint32_t at = 0; at < parents->size(); ++at) { groups[(*parents)[at]].push_back(at); }
  std::vector<WaterSurfaceTopology> result;
  result.reserve(groups.size());
  for (const auto &[root, faces] : groups) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    auto part = ExtractComponent(topology, faces);
    if (!part) { return std::unexpected(part.error()); }
    result.push_back(std::move(*part));
  }
  return result;
}
}
