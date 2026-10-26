#include "WaterSurfaceTopology.h"
#include "Geodesy.h"
#include "GeosGeometry.h"
#include "Units.h"
#include <geos_c.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = WaterTopologyError;
using Point = std::array<double, 2>;
using Geometry = Spatial::GeosGeometry;

struct Context {
  GEOSContextHandle_t Handle = GEOS_init_r();
  Context() = default;
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  ~Context() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }
};

struct DestroyCoordinates {
  GEOSContextHandle_t Context;

  void operator()(GEOSCoordSequence *sequence) const { GEOSCoordSeq_destroy_r(Context, sequence); }
};

struct DestroyTree {
  GEOSContextHandle_t Context;

  void operator()(GEOSSTRtree *tree) const { GEOSSTRtree_destroy_r(Context, tree); }
};

Geometry Own(GEOSContextHandle_t context, GEOSGeometry *geometry) {
  return Geometry(geometry, Spatial::DestroyGeosGeometry{context});
}

Geometry BuildSourceRing(GEOSContextHandle_t context,
                         std::span<const double> points,
                         WaterAsset::SurfaceRing ring,
                         const TangentFrame &frame) {
  if (ring.PointCount < 3 || ring.FirstPoint > points.size() / 2 ||
      ring.PointCount > points.size() / 2 - ring.FirstPoint ||
      ring.PointCount == std::numeric_limits<uint32_t>::max()) {
    return Own(context, nullptr);
  }
  const size_t begin = static_cast<size_t>(ring.FirstPoint) * 2;
  const size_t end = begin + static_cast<size_t>(ring.PointCount - 1) * 2;
  const bool closed = points[begin] == points[end] && points[begin + 1] == points[end + 1];
  const uint32_t count = ring.PointCount + (closed ? 0 : 1);
  std::unique_ptr<GEOSCoordSequence, DestroyCoordinates> coordinates(
      GEOSCoordSeq_create_r(context, count, 2), DestroyCoordinates{context});
  if (!coordinates) { return Own(context, nullptr); }
  for (uint32_t at = 0; at < count; ++at) {
    const size_t point = begin + static_cast<size_t>(at % ring.PointCount) * 2;
    const LongitudeLatitude geographic{.LongitudeDeg = points[point + 1],
                                       .LatitudeDeg = points[point]};
    if (!std::isfinite(geographic.LongitudeDeg) || !std::isfinite(geographic.LatitudeDeg) ||
        std::abs(geographic.LongitudeDeg) > kDegPerHalfTurn ||
        std::abs(geographic.LatitudeDeg) > kDegPerHalfTurn / 2) {
      return Own(context, nullptr);
    }
    const auto axes = EnuAxesEcef(
        {.LongitudeDeg = geographic.LongitudeDeg, .LatitudeDeg = geographic.LatitudeDeg});
    if (frame.ToLocalDirection(axes.Up).UpM <= 0) { return Own(context, nullptr); }
    const auto xy = frame.ToLocalGroundPosition(geographic);
    if (GEOSCoordSeq_setXY_r(context, coordinates.get(), at, xy.EastM, xy.NorthM) == 0) {
      return Own(context, nullptr);
    }
  }
  return Own(context, GEOSGeom_createLinearRing_r(context, coordinates.release()));
}

Geometry BuildSourcePolygon(GEOSContextHandle_t context,
                            const WaterAsset &water,
                            const WaterAsset::Surface &surface,
                            const TangentFrame &frame) {
  const auto rings = water.RingsOf(surface);
  if (rings.empty()) { return Own(context, nullptr); }
  auto shell = BuildSourceRing(context, water.Points(), rings.front(), frame);
  if (!shell) { return shell; }
  std::vector<Geometry> holes;
  for (const auto &ring : rings.subspan(1)) {
    holes.push_back(BuildSourceRing(context, water.Points(), ring, frame));
    if (!holes.back()) { return Own(context, nullptr); }
  }
  std::vector<GEOSGeometry *> raw;
  raw.reserve(holes.size());
  for (auto &hole : holes) { raw.push_back(hole.release()); }
  return Own(context,
             GEOSGeom_createPolygon_r(
                 context, shell.release(), raw.data(), static_cast<unsigned>(raw.size())));
}

std::expected<Geometry, Error> BuildSourceCollection(GEOSContextHandle_t context,
                                                     const WaterAsset &water,
                                                     std::span<const uint32_t> selected,
                                                     const TangentFrame &frame,
                                                     const WaterTopologyBudget &budget,
                                                     const std::stop_token &stop) {
  std::vector<Geometry> polygons;
  size_t coordinates = 0;
  for (const auto index : selected) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    for (const auto &ring : water.RingsOf(water.Surfaces()[index])) {
      if (ring.PointCount > budget.SourceCoordinates - coordinates) {
        return std::unexpected(Error::CapacityExceeded);
      }
      coordinates += ring.PointCount;
    }
    auto polygon = BuildSourcePolygon(context, water, water.Surfaces()[index], frame);
    if (!polygon || GEOSisValid_r(context, polygon.get()) != 1 ||
        GEOSisEmpty_r(context, polygon.get()) != 0) {
      return std::unexpected(Error::InvalidGeometry);
    }
    polygons.push_back(std::move(polygon));
  }
  std::vector<GEOSGeometry *> raw;
  raw.reserve(polygons.size());
  for (auto &polygon : polygons) { raw.push_back(polygon.release()); }
  auto collection =
      Own(context,
          GEOSGeom_createCollection_r(
              context, GEOS_GEOMETRYCOLLECTION, raw.data(), static_cast<unsigned>(raw.size())));
  if (!collection) { return std::unexpected(Error::LibraryFailure); }
  return collection;
}

std::expected<size_t, Error> RefinedRingVertices(GEOSContextHandle_t context,
                                                 const GEOSGeometry *ring,
                                                 const WaterTopologyBudget &budget,
                                                 size_t maximum) {
  const auto *sequence = GEOSGeom_getCoordSeq_r(context, ring);
  unsigned count = 0;
  if (sequence == nullptr || GEOSCoordSeq_getSize_r(context, sequence, &count) == 0) {
    return std::unexpected(Error::LibraryFailure);
  }
  double vertices = 0;
  Point previous{};
  for (unsigned point = 0; point < count; ++point) {
    Point next{};
    if (GEOSCoordSeq_getXY_r(context, sequence, point, next.data(), &next[1]) == 0) {
      return std::unexpected(Error::LibraryFailure);
    }
    if (point != 0) {
      vertices += std::max(1.0,
                           std::ceil(std::hypot(next[0] - previous[0], next[1] - previous[1]) /
                                     budget.BoundarySpacingM));
    }
    previous = next;
    if (vertices > static_cast<double>(maximum)) {
      return std::unexpected(Error::CapacityExceeded);
    }
  }
  return static_cast<size_t>(vertices);
}

std::expected<void, Error> CheckBoundaryBudget(GEOSContextHandle_t context,
                                               const GEOSGeometry *body,
                                               const WaterTopologyBudget &budget) {
  const auto kind = GEOSGeomTypeId_r(context, body);
  if (kind != GEOS_POLYGON && kind != GEOS_MULTIPOLYGON) {
    return std::unexpected(Error::InvalidGeometry);
  }
  const int parts = GEOSGetNumGeometries_r(context, body);
  if (parts <= 0) { return std::unexpected(Error::InvalidGeometry); }
  size_t vertices = 0;
  size_t holes = 0;
  for (int part = 0; part < parts; ++part) {
    const auto *polygon = GEOSGetGeometryN_r(context, body, part);
    const int inside = GEOSGetNumInteriorRings_r(context, polygon);
    if (inside < 0) { return std::unexpected(Error::LibraryFailure); }
    holes += static_cast<size_t>(inside);
    for (int ring = -1; ring < inside; ++ring) {
      const auto *shape = ring == -1 ? GEOSGetExteriorRing_r(context, polygon)
                                     : GEOSGetInteriorRingN_r(context, polygon, ring);
      const auto count = RefinedRingVertices(context, shape, budget, budget.Vertices - vertices);
      if (!count) { return std::unexpected(count.error()); }
      vertices += *count;
    }
  }
  const auto faces = vertices + 2 * holes - 2 * static_cast<size_t>(parts);
  if (faces > budget.Faces) { return std::unexpected(Error::CapacityExceeded); }
  return {};
}

std::expected<std::array<Point, 3>, Error> ReadTriangle(GEOSContextHandle_t context,
                                                        const GEOSGeometry *triangle) {
  const auto *ring = GEOSGetExteriorRing_r(context, triangle);
  const auto *sequence = ring != nullptr ? GEOSGeom_getCoordSeq_r(context, ring) : nullptr;
  unsigned count = 0;
  if (sequence == nullptr || GEOSCoordSeq_getSize_r(context, sequence, &count) == 0 || count != 4) {
    return std::unexpected(Error::LibraryFailure);
  }
  std::array<Point, 3> points{};
  for (unsigned at = 0; at < 3; ++at) {
    if (GEOSCoordSeq_getXY_r(context, sequence, at, points[at].data(), &points[at][1]) == 0 ||
        !std::isfinite(points[at][0]) || !std::isfinite(points[at][1])) {
      return std::unexpected(Error::LibraryFailure);
    }
  }
  return points;
}

struct SourcePolygon {
  uint32_t Index;
  const GEOSGeometry *Shape;
  bool Covered = false;
};

void Candidate(void *item, void *into) {
  static_cast<std::vector<SourcePolygon *> *>(into)->push_back(static_cast<SourcePolygon *>(item));
}

std::expected<double, Error>
OverlapArea(GEOSContextHandle_t context, const GEOSGeometry *first, const GEOSGeometry *second) {
  const auto cut = Own(context, GEOSIntersection_r(context, first, second));
  double area = 0;
  if (!cut || GEOSArea_r(context, cut.get(), &area) == 0 || !std::isfinite(area)) {
    return std::unexpected(Error::LibraryFailure);
  }
  return area;
}

std::expected<void, Error> BindSources(GEOSContextHandle_t context,
                                       const GEOSGeometry *collection,
                                       const GEOSGeometry *triangles,
                                       std::span<const uint32_t> selected,
                                       WaterSurfaceTopology &topology,
                                       const WaterTopologyBudget &budget,
                                       const std::stop_token &stop) {
  std::vector<SourcePolygon> sources;
  sources.reserve(selected.size());
  for (size_t at = 0; at < selected.size(); ++at) {
    sources.push_back({.Index = selected[at],
                       .Shape = GEOSGetGeometryN_r(context, collection, static_cast<int>(at))});
  }
  const std::unique_ptr<GEOSSTRtree, DestroyTree> tree(GEOSSTRtree_create_r(context, 16),
                                                       DestroyTree{context});
  if (!tree) { return std::unexpected(Error::LibraryFailure); }
  for (auto &source : sources) { GEOSSTRtree_insert_r(context, tree.get(), source.Shape, &source); }
  std::vector<SourcePolygon *> candidates;
  candidates.reserve(sources.size());
  topology.FaceSourceOffsets.push_back(0);
  for (size_t face = 0; face < topology.Faces.size(); ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto *triangle = GEOSGetGeometryN_r(context, triangles, static_cast<int>(face));
    candidates.clear();
    GEOSSTRtree_query_r(context, tree.get(), triangle, Candidate, &candidates);
    if (candidates.size() > budget.Intersections - topology.Intersections) {
      return std::unexpected(Error::CapacityExceeded);
    }
    topology.Intersections += candidates.size();
    std::ranges::sort(candidates, {}, [](const auto *candidate) { return candidate->Index; });
    double largest = 0;
    const auto first = topology.FaceSources.size();
    for (auto *source : candidates) {
      const auto area = OverlapArea(context, triangle, source->Shape);
      if (!area) { return std::unexpected(area.error()); }
      if (*area <= 0) { continue; }
      if (topology.FaceSources.size() >= budget.Memberships) {
        return std::unexpected(Error::CapacityExceeded);
      }
      topology.FaceSources.push_back(source->Index);
      source->Covered = true;
      if (*area > largest) {
        largest = *area;
        topology.Faces[face].Surface = source->Index;
      }
    }
    if (first == topology.FaceSources.size()) { return std::unexpected(Error::InvalidGeometry); }
    topology.FaceSourceOffsets.push_back(static_cast<uint32_t>(topology.FaceSources.size()));
  }
  if (std::ranges::any_of(sources, [](const auto &source) { return !source.Covered; })) {
    return std::unexpected(Error::InvalidGeometry);
  }
  return {};
}

std::expected<WaterSurfaceTopology, Error> ReadTopology(GEOSContextHandle_t context,
                                                        const GEOSGeometry *triangles,
                                                        const WaterTopologyBudget &budget) {
  const int count = GEOSGetNumGeometries_r(context, triangles);
  if (count <= 0) { return std::unexpected(Error::InvalidGeometry); }
  if (std::cmp_greater(count, budget.Faces)) { return std::unexpected(Error::CapacityExceeded); }
  std::map<Point, uint32_t> indices;
  for (int face = 0; face < count; ++face) {
    const auto points = ReadTriangle(context, GEOSGetGeometryN_r(context, triangles, face));
    if (!points) { return std::unexpected(points.error()); }
    for (const auto &point : *points) { indices.try_emplace(point, 0); }
    if (indices.size() > budget.Vertices) { return std::unexpected(Error::CapacityExceeded); }
  }
  WaterSurfaceTopology result;
  result.VerticesXY.reserve(indices.size());
  for (auto &[point, index] : indices) {
    index = static_cast<uint32_t>(result.VerticesXY.size());
    result.VerticesXY.push_back(point);
  }
  result.Faces.reserve(static_cast<size_t>(count));
  for (int face = 0; face < count; ++face) {
    const auto points = ReadTriangle(context, GEOSGetGeometryN_r(context, triangles, face));
    if (!points) { return std::unexpected(points.error()); }
    const auto &[first, second, third] = *points;
    const double area = (second[0] - first[0]) * (third[1] - first[1]) -
                        (second[1] - first[1]) * (third[0] - first[0]);
    if (!std::isfinite(area) || area == 0) { return std::unexpected(Error::InvalidGeometry); }
    std::array vertices{
        indices.find(first)->second, indices.find(second)->second, indices.find(third)->second};
    if (area < 0) { std::swap(vertices[1], vertices[2]); }
    result.Faces.push_back({.Vertices = vertices});
  }
  return result;
}
}

std::expected<WaterSurfaceTopology, WaterTopologyError>
BuildWaterSurfaceTopology(const WaterAsset &water,
                          std::span<const uint32_t> admittedSurfaces,
                          const TangentFrame &frame,
                          WaterTopologyBudget budget,
                          const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (admittedSurfaces.empty() || !std::isfinite(budget.BoundarySpacingM) ||
      budget.BoundarySpacingM <= 0 || !std::isfinite(frame.Anchor().LongitudeDeg) ||
      !std::isfinite(frame.Anchor().LatitudeDeg) ||
      std::abs(frame.Anchor().LatitudeDeg) > kDegPerHalfTurn / 2 ||
      std::abs(frame.Anchor().LongitudeDeg) > kDegPerHalfTurn) {
    return std::unexpected(Error::InvalidInput);
  }
  if (admittedSurfaces.size() > budget.Sources) { return std::unexpected(Error::CapacityExceeded); }
  std::vector<uint32_t> selected(admittedSurfaces.begin(), admittedSurfaces.end());
  std::ranges::sort(selected);
  if (selected.back() >= water.Surfaces().size() ||
      std::ranges::adjacent_find(selected) != selected.end()) {
    return std::unexpected(Error::InvalidInput);
  }
  const Context context;
  if (context.Handle == nullptr) { return std::unexpected(Error::LibraryFailure); }
  auto sources = BuildSourceCollection(context.Handle, water, selected, frame, budget, stop);
  if (!sources) { return std::unexpected(sources.error()); }
  const auto body = Own(context.Handle, GEOSUnaryUnion_r(context.Handle, sources->get()));
  if (!body || GEOSisValid_r(context.Handle, body.get()) != 1) {
    return std::unexpected(Error::InvalidGeometry);
  }
  const auto permitted = CheckBoundaryBudget(context.Handle, body.get(), budget);
  if (!permitted) { return std::unexpected(permitted.error()); }
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto boundary =
      Own(context.Handle, GEOSDensify_r(context.Handle, body.get(), budget.BoundarySpacingM));
  if (!boundary) { return std::unexpected(Error::LibraryFailure); }
  const auto triangles =
      Own(context.Handle, GEOSConstrainedDelaunayTriangulation_r(context.Handle, boundary.get()));
  if (!triangles) { return std::unexpected(Error::LibraryFailure); }
  auto result = ReadTopology(context.Handle, triangles.get(), budget);
  if (!result) { return std::unexpected(result.error()); }
  result->Anchor = frame.Anchor();
  if (GEOSArea_r(context.Handle, body.get(), &result->AreaM2) == 0 ||
      !std::isfinite(result->AreaM2) || result->AreaM2 <= 0) {
    return std::unexpected(Error::InvalidGeometry);
  }
  const auto memberships =
      BindSources(context.Handle, sources->get(), triangles.get(), selected, *result, budget, stop);
  if (!memberships) { return std::unexpected(memberships.error()); }
  return result;
}
}
