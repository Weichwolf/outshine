#include "WaterSurfaceMesh.h"
#include "math/Box.h"
#include "math/Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
bool ValidAnchor(LongitudeLatitude at) {
  return std::isfinite(at.LongitudeDeg) && std::isfinite(at.LatitudeDeg) &&
         std::abs(at.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(at.LatitudeDeg) <= kDegPerHalfTurn / 2;
}

std::expected<std::vector<Box>, WaterMeshError>
FaceBounds(LongitudeLatitude anchor,
           std::span<const Vec3> vertices,
           std::span<const WaterSurfaceMesh::Face> faces) {
  if (vertices.size() > std::numeric_limits<uint32_t>::max() ||
      faces.size() > std::numeric_limits<uint32_t>::max() / 2u) {
    return std::unexpected(WaterMeshError::CapacityExceeded);
  }
  if (!ValidAnchor(anchor) || vertices.empty() != faces.empty() ||
      !std::ranges::all_of(vertices, [](const Vec3 &point) {
        return std::ranges::all_of(point, [](double v) { return std::isfinite(v); });
      })) {
    return std::unexpected(WaterMeshError::InvalidGeometry);
  }
  std::vector<Box> bounds;
  bounds.reserve(faces.size());
  for (const auto &face : faces) {
    if (std::ranges::any_of(face.Vertices, [&](uint32_t i) { return i >= vertices.size(); })) {
      return std::unexpected(WaterMeshError::InvalidGeometry);
    }
    const Vec3 normal = Cross(vertices[face.Vertices[1]] - vertices[face.Vertices[0]],
                              vertices[face.Vertices[2]] - vertices[face.Vertices[0]]);
    if (!(normal[2] > 0) ||
        !std::ranges::all_of(normal, [](double v) { return std::isfinite(v); })) {
      return std::unexpected(WaterMeshError::InvalidGeometry);
    }
    Box box;
    for (const uint32_t i : face.Vertices) { box.Cover(vertices[i]); }
    bounds.push_back(box);
  }
  return bounds;
}
}

std::expected<WaterSurfaceMesh, WaterMeshError> WaterSurfaceMesh::Build(
    LongitudeLatitude anchor, std::vector<Vec3> verticesEnuM, std::vector<Face> faces) {
  const auto bounds = FaceBounds(anchor, verticesEnuM, faces);
  if (!bounds) { return std::unexpected(bounds.error()); }
  auto index = PlanHierarchy::Build(*bounds);
  if (!index) { return std::unexpected(WaterMeshError::CapacityExceeded); }
  WaterSurfaceMesh mesh;
  mesh.Frame_ = TangentFrame::At(anchor);
  mesh.Vertices_ = std::move(verticesEnuM);
  mesh.Faces_ = std::move(faces);
  mesh.Hierarchy_ = std::move(*index);
  return mesh;
}

std::expected<WaterSurfaceMesh, WaterMeshError>
WaterSurfaceMesh::Restore(LongitudeLatitude anchor,
                          std::vector<Vec3> verticesEnuM,
                          std::vector<Face> faces,
                          std::vector<PlanHierarchy::Node> nodes,
                          std::vector<uint32_t> order) {
  const auto bounds = FaceBounds(anchor, verticesEnuM, faces);
  if (!bounds) { return std::unexpected(bounds.error()); }
  auto index = PlanHierarchy::Restore(std::move(nodes), std::move(order), *bounds);
  if (!index) { return std::unexpected(WaterMeshError::InvalidStorage); }
  WaterSurfaceMesh mesh;
  mesh.Frame_ = TangentFrame::At(anchor);
  mesh.Vertices_ = std::move(verticesEnuM);
  mesh.Faces_ = std::move(faces);
  mesh.Hierarchy_ = std::move(*index);
  return mesh;
}

size_t WaterSurfaceMesh::HeapBytes() const noexcept {
  return Vertices_.capacity() * sizeof(Vec3) + Faces_.capacity() * sizeof(Face) +
         Hierarchy_.HeapBytes();
}
}
