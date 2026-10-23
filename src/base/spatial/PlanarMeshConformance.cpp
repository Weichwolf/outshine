#include "PlanarMeshInternal.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine {
namespace {
using namespace PlanarMeshDetail;

std::expected<void, PlanarMeshError> Validate(std::span<const PlanarPoint> points,
                                              std::span<const PlanarFace> faces,
                                              double weldDistance,
                                              PlanarMeshBudget budget) {
  if (!std::isfinite(weldDistance) || weldDistance < 0) {
    return std::unexpected(PlanarMeshError::InvalidInput);
  }
  if (points.size() > budget.Vertices || faces.size() > budget.Faces ||
      points.size() > std::numeric_limits<uint32_t>::max() ||
      faces.size() > std::numeric_limits<uint32_t>::max() ||
      faces.size() > std::numeric_limits<size_t>::max() / 3) {
    return std::unexpected(PlanarMeshError::CapacityExceeded);
  }
  const double coordinateLimit = std::sqrt(std::numeric_limits<double>::max()) / 8;
  for (const auto point : points) {
    for (const double value : point) {
      if (!std::isfinite(value) || std::abs(value) > coordinateLimit) {
        return std::unexpected(PlanarMeshError::InvalidInput);
      }
    }
  }
  return {};
}

std::expected<void, PlanarMeshError> PrepareFaces(PlanarMesh &mesh,
                                                  std::span<const PlanarFace> faces) {
  for (uint32_t at = 0; at < faces.size(); ++at) {
    auto face = faces[at];
    for (auto &point : face) {
      if (point >= mesh.PointRemap.size()) {
        return std::unexpected(PlanarMeshError::InvalidInput);
      }
      point = mesh.PointRemap[point];
    }
    const double area = Area(mesh.Points[face[0]], mesh.Points[face[1]], mesh.Points[face[2]]);
    if (!std::isfinite(area)) { return std::unexpected(PlanarMeshError::InvalidInput); }
    if (area == 0) {
      ++mesh.Work.RemovedFaces;
      continue;
    }
    if (area < 0) { std::ranges::reverse(face); }
    mesh.Faces.push_back(face);
    mesh.FaceParents.push_back(at);
  }
  return {};
}

std::vector<Edge> EdgesOf(std::span<const PlanarFace> faces) {
  std::vector<Edge> edges;
  edges.reserve(faces.size() * 3);
  for (const auto face : faces) {
    for (size_t at = 0; at < 3; ++at) {
      const auto a = face[at];
      const auto b = face[(at + 1) % 3];
      edges.push_back({std::min(a, b), std::max(a, b)});
    }
  }
  std::ranges::sort(edges);
  edges.erase(std::ranges::unique(edges).begin(), edges.end());
  return edges;
}

std::expected<void, PlanarMeshError> SplitEdge(std::vector<PlanarFace> &pieces,
                                               uint32_t a,
                                               uint32_t b,
                                               std::span<const Split> splits,
                                               size_t maximum) {
  if (splits.empty()) { return {}; }
  const auto owner = std::ranges::find_if(pieces, [&](const PlanarFace &face) {
    return std::ranges::find(face, a) != face.end() && std::ranges::find(face, b) != face.end();
  });
  assert(owner != pieces.end());
  if (pieces.size() > maximum || splits.size() > maximum - pieces.size()) {
    return std::unexpected(PlanarMeshError::CapacityExceeded);
  }
  const uint32_t opposite =
      *std::ranges::find_if(*owner, [&](uint32_t p) { return p != a && p != b; });
  const size_t at = static_cast<size_t>(owner - pieces.begin());
  std::vector<PlanarFace> replacement;
  replacement.reserve(splits.size() + 1);
  uint32_t previous = a;
  for (size_t index = 0; index < splits.size(); ++index) {
    const uint32_t point = splits[a < b ? index : splits.size() - 1 - index].Point;
    replacement.push_back({previous, point, opposite});
    previous = point;
  }
  replacement.push_back({previous, b, opposite});
  pieces.erase(pieces.begin() + static_cast<std::ptrdiff_t>(at));
  pieces.insert(
      pieces.begin() + static_cast<std::ptrdiff_t>(at), replacement.begin(), replacement.end());
  return {};
}

std::expected<void, PlanarMeshError> SplitFace(const PlanarFace &original,
                                               std::span<const Edge> edges,
                                               std::span<const Split> splits,
                                               std::vector<PlanarFace> &pieces,
                                               size_t maximum) {
  for (size_t at = 0; at < 3; ++at) {
    const auto a = original[at];
    const auto b = original[(at + 1) % 3];
    const Edge edge{std::min(a, b), std::max(a, b)};
    const auto found = std::ranges::lower_bound(edges, edge);
    assert(found != edges.end() && *found == edge);
    const auto key = static_cast<uint32_t>(found - edges.begin());
    const auto range = std::ranges::equal_range(splits, key, {}, &Split::EdgeIndex);
    if (range.empty()) { continue; }
    if (pieces.empty()) { pieces.push_back(original); }
    if (auto result = SplitEdge(pieces, a, b, {range.begin(), range.end()}, maximum); !result) {
      return result;
    }
  }
  return {};
}

std::expected<void, PlanarMeshError> ApplySplits(PlanarMesh &mesh,
                                                 std::span<const Edge> edges,
                                                 std::span<const Split> splits,
                                                 PlanarMeshBudget budget,
                                                 const std::stop_token &stop) {
  std::vector<PlanarFace> output;
  std::vector<uint32_t> parents;
  std::vector<PlanarFace> pieces;
  output.reserve(mesh.Faces.size());
  parents.reserve(mesh.FaceParents.size());
  for (size_t index = 0; index < mesh.Faces.size(); ++index) {
    if (stop.stop_requested()) { return std::unexpected(PlanarMeshError::Canceled); }
    const auto original = mesh.Faces[index];
    pieces.clear();
    if (auto result = SplitFace(original, edges, splits, pieces, budget.Faces - output.size());
        !result) {
      return result;
    }
    const std::span<const PlanarFace> added = pieces.empty()
                                                  ? std::span<const PlanarFace>(&original, 1)
                                                  : std::span<const PlanarFace>(pieces);
    for (const auto face : added) {
      const double area = Area(mesh.Points[face[0]], mesh.Points[face[1]], mesh.Points[face[2]]);
      if (!std::isfinite(area)) { return std::unexpected(PlanarMeshError::InvalidInput); }
      if (area == 0) {
        ++mesh.Work.RemovedFaces;
        continue;
      }
      if (output.size() == budget.Faces) {
        return std::unexpected(PlanarMeshError::CapacityExceeded);
      }
      output.push_back(face);
      parents.push_back(mesh.FaceParents[index]);
    }
  }
  mesh.Faces = std::move(output);
  mesh.FaceParents = std::move(parents);
  return {};
}
}

std::expected<PlanarMesh, PlanarMeshError> ConformPlanarMesh(std::span<const PlanarPoint> points,
                                                             std::span<const PlanarFace> faces,
                                                             double weldDistance,
                                                             PlanarMeshBudget budget,
                                                             const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(PlanarMeshError::Canceled); }
  if (auto valid = Validate(points, faces, weldDistance, budget); !valid) {
    return std::unexpected(valid.error());
  }
  PlanarMesh mesh;
  if (auto result = Weld(mesh, points, weldDistance, budget, stop); !result) {
    return std::unexpected(result.error());
  }
  double scale = 1;
  for (const auto point : mesh.Points) {
    scale = std::max({scale, std::abs(point[0]), std::abs(point[1])});
  }
  mesh.Work.EdgeTolerance = 64 * std::numeric_limits<double>::epsilon() * scale;
  if (auto result = PrepareFaces(mesh, faces); !result) { return std::unexpected(result.error()); }
  while (!mesh.Faces.empty()) {
    if (stop.stop_requested()) { return std::unexpected(PlanarMeshError::Canceled); }
    const auto edges = EdgesOf(mesh.Faces);
    const auto splits = FindSplits(mesh, edges, budget, stop);
    if (!splits) { return std::unexpected(splits.error()); }
    if (splits->empty()) { break; }
    ++mesh.Work.Passes;
    uint32_t previous = std::numeric_limits<uint32_t>::max();
    for (const auto &split : *splits) {
      if (split.EdgeIndex != previous) { ++mesh.Work.SplitEdges; }
      previous = split.EdgeIndex;
    }
    if (auto result = ApplySplits(mesh, edges, *splits, budget, stop); !result) {
      return std::unexpected(result.error());
    }
  }
  return mesh;
}
}
