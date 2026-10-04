#include "PolygonTriangulation.h"

#include <geos_c.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {

struct Context {
  GEOSContextHandle_t Handle = GEOS_init_r();

  Context() = default;
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  ~Context() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }
};

struct DestroyGeometry {
  GEOSContextHandle_t Context;

  void operator()(GEOSGeometry *geometry) const { GEOSGeom_destroy_r(Context, geometry); }
};

struct DestroyCoordinates {
  GEOSContextHandle_t Context;

  void operator()(GEOSCoordSequence *sequence) const { GEOSCoordSeq_destroy_r(Context, sequence); }
};

using Geometry = std::unique_ptr<GEOSGeometry, DestroyGeometry>;
using VertexIndices = std::map<std::array<double, 2>, uint32_t>;

Geometry MakeRing(GEOSContextHandle_t context, const PolygonRing &ring) {
  const bool closed = ring.front() == ring.back();
  const auto count = static_cast<unsigned int>(ring.size() + (closed ? 0u : 1u));
  std::unique_ptr<GEOSCoordSequence, DestroyCoordinates> coordinates(
      GEOSCoordSeq_create_r(context, count, 2), DestroyCoordinates{context});
  if (!coordinates) { return Geometry(nullptr, DestroyGeometry{context}); }
  for (unsigned int at = 0; at < count; ++at) {
    const auto &point = ring[at % ring.size()];
    if (GEOSCoordSeq_setXY_r(context, coordinates.get(), at, point[0], point[1]) == 0) {
      return Geometry(nullptr, DestroyGeometry{context});
    }
  }
  return Geometry(GEOSGeom_createLinearRing_r(context, coordinates.release()),
                  DestroyGeometry{context});
}

Geometry MakePolygon(GEOSContextHandle_t context, std::span<const PolygonRing> rings) {
  Geometry shell = MakeRing(context, rings.front());
  if (!shell) { return shell; }
  std::vector<Geometry> holes;
  for (const auto &ring : rings.subspan(1)) {
    holes.push_back(MakeRing(context, ring));
    if (!holes.back()) { return Geometry(nullptr, DestroyGeometry{context}); }
  }
  std::vector<GEOSGeometry *> inner;
  inner.reserve(holes.size());
  for (auto &hole : holes) { inner.push_back(hole.release()); }
  return Geometry(
      GEOSGeom_createPolygon_r(
          context, shell.release(), inner.data(), static_cast<unsigned int>(inner.size())),
      DestroyGeometry{context});
}

std::expected<VertexIndices, PolygonTriangulationError>
IndexVertices(std::span<const PolygonRing> rings) {
  if (rings.empty()) { return std::unexpected(PolygonTriangulationError::InvalidPolygon); }
  VertexIndices vertices;
  uint64_t count = 0;
  for (const auto &ring : rings) {
    if (ring.size() < 3) { return std::unexpected(PolygonTriangulationError::InvalidPolygon); }
    if (ring.size() >= std::numeric_limits<unsigned int>::max() ||
        ring.size() > std::numeric_limits<uint32_t>::max() - count) {
      return std::unexpected(PolygonTriangulationError::CapacityExceeded);
    }
    for (const auto &point : ring) {
      if (!std::isfinite(point[0]) || !std::isfinite(point[1])) {
        return std::unexpected(PolygonTriangulationError::InvalidPolygon);
      }
      vertices.try_emplace(point, static_cast<uint32_t>(count++));
    }
  }
  return vertices;
}

std::expected<std::vector<uint32_t>, PolygonTriangulationError> ReadTriangles(
    GEOSContextHandle_t context, const GEOSGeometry *triangles, const VertexIndices &vertices) {
  const int count = GEOSGetNumGeometries_r(context, triangles);
  if (count <= 0) { return std::unexpected(PolygonTriangulationError::LibraryFailure); }
  std::vector<uint32_t> result;
  result.reserve(static_cast<size_t>(count) * 3);
  for (int at = 0; at < count; ++at) {
    const auto *triangle = GEOSGetGeometryN_r(context, triangles, at);
    if (triangle == nullptr) { return std::unexpected(PolygonTriangulationError::LibraryFailure); }
    const auto *ring = GEOSGetExteriorRing_r(context, triangle);
    if (ring == nullptr) { return std::unexpected(PolygonTriangulationError::LibraryFailure); }
    const auto *coordinates = GEOSGeom_getCoordSeq_r(context, ring);
    unsigned int points = 0;
    if (coordinates == nullptr || GEOSCoordSeq_getSize_r(context, coordinates, &points) == 0 ||
        points != 4) {
      return std::unexpected(PolygonTriangulationError::LibraryFailure);
    }
    for (unsigned int corner = 0; corner < 3; ++corner) {
      std::array<double, 2> point{};
      if (GEOSCoordSeq_getXY_r(context, coordinates, corner, point.data(), &point[1]) == 0) {
        return std::unexpected(PolygonTriangulationError::LibraryFailure);
      }
      const auto found = vertices.find(point);
      if (found == vertices.end()) {
        return std::unexpected(PolygonTriangulationError::LibraryFailure);
      }
      result.push_back(found->second);
    }
  }
  return result;
}
}

std::expected<std::vector<uint32_t>, PolygonTriangulationError>
TriangulatePolygon(std::span<const PolygonRing> rings) {
  const auto vertices = IndexVertices(rings);
  if (!vertices) { return std::unexpected(vertices.error()); }
  const Context context;
  if (context.Handle == nullptr) {
    return std::unexpected(PolygonTriangulationError::LibraryFailure);
  }
  const auto polygon = MakePolygon(context.Handle, rings);
  if (!polygon) { return std::unexpected(PolygonTriangulationError::LibraryFailure); }
  const char valid = GEOSisValid_r(context.Handle, polygon.get());
  if (valid == 2) { return std::unexpected(PolygonTriangulationError::LibraryFailure); }
  if (valid == 0) { return std::unexpected(PolygonTriangulationError::InvalidPolygon); }
  const Geometry triangles(GEOSConstrainedDelaunayTriangulation_r(context.Handle, polygon.get()),
                           DestroyGeometry{context.Handle});
  if (!triangles) { return std::unexpected(PolygonTriangulationError::LibraryFailure); }
  return ReadTriangles(context.Handle, triangles.get(), *vertices);
}

}
