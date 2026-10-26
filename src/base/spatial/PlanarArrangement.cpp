#include "PlanarArrangementInternal.h"
#include "GeosPolygon.h"
#include <geos_c.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::PlanarArrangements {
Context::Context(const std::stop_token &stop) : Stop(stop) {
#if GEOS_VERSION_MAJOR > 3 || GEOS_VERSION_MINOR >= 14
  if (Handle != nullptr) {
    GEOSContext_setInterruptCallback_r(
        Handle,
        [](void *data) {
          return static_cast<const Context *>(data)->Stop.stop_requested() ? 1 : 0;
        },
        this);
  }
#endif
}

Context::~Context() {
  if (Handle != nullptr) { GEOS_finish_r(Handle); }
}

Geometry Collection(const Context &context, std::vector<Geometry> parts) {
  std::vector<GEOSGeometry *> raw;
  raw.reserve(parts.size());
  for (auto &part : parts) { raw.push_back(part.release()); }
  return context.Own(GEOSGeom_createCollection_r(
      context.Handle, GEOS_GEOMETRYCOLLECTION, raw.data(), static_cast<unsigned int>(raw.size())));
}

namespace {
Checked<void> CountLine(std::span<const PlanarPoint> points,
                        bool ring,
                        PlanarArrangementWork &work,
                        size_t maximum) {
  if (points.size() < (ring ? 3u : 2u)) { return std::unexpected(Error::InvalidInput); }
  const size_t segments = points.size() - 1 + (ring && points.front() != points.back() ? 1 : 0);
  if (segments > maximum - work.InputSegments) { return std::unexpected(Error::CapacityExceeded); }
  work.InputSegments += segments;
  const double coordinateBound = std::sqrt(std::numeric_limits<double>::max()) / 8;
  for (const auto &point : points) {
    for (const auto value : point) {
      if (!std::isfinite(value) || std::abs(value) > coordinateBound) {
        return std::unexpected(Error::InvalidInput);
      }
    }
  }
  return {};
}

Checked<void> Validate(std::span<const PlanarDomain> domains,
                       std::span<const PlanarLine> lines,
                       PlanarArrangementRecipe recipe,
                       PlanarArrangementBudget budget,
                       PlanarArrangementWork &work) {
  if (!std::isfinite(recipe.WeldDistanceM) || recipe.WeldDistanceM < 0 ||
      !std::isfinite(recipe.MaximumAreaDifferenceM2) || recipe.MaximumAreaDifferenceM2 < 0 ||
      (domains.empty() && !lines.empty())) {
    return std::unexpected(Error::InvalidInput);
  }
  if (domains.size() > std::numeric_limits<uint64_t>::digits ||
      budget.InputSegments >= std::numeric_limits<unsigned int>::max() ||
      budget.NodedSegments >= std::numeric_limits<unsigned int>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  for (const auto &domain : domains) {
    if (domain.Rings.empty()) { return std::unexpected(Error::InvalidInput); }
    for (const auto &ring : domain.Rings) {
      if (auto result = CountLine(ring, true, work, budget.InputSegments); !result) {
        return result;
      }
    }
  }
  for (const auto &line : lines) {
    if (auto result = CountLine(line, false, work, budget.InputSegments); !result) {
      return result;
    }
  }
  return {};
}

Geometry MakeLine(const Context &context, std::span<const PlanarPoint> points) {
  auto *coordinates =
      GEOSCoordSeq_create_r(context.Handle, static_cast<unsigned int>(points.size()), 2);
  if (coordinates == nullptr) { return context.Own(nullptr); }
  for (size_t at = 0; at < points.size(); ++at) {
    if (GEOSCoordSeq_setXY_r(context.Handle,
                             coordinates,
                             static_cast<unsigned int>(at),
                             points[at][0],
                             points[at][1]) == 0) {
      GEOSCoordSeq_destroy_r(context.Handle, coordinates);
      return context.Own(nullptr);
    }
  }
  return context.Own(GEOSGeom_createLineString_r(context.Handle, coordinates));
}

Checked<std::vector<Geometry>> MakeDomains(const Context &context,
                                           std::span<const PlanarDomain> domains) {
  std::vector<Geometry> result;
  result.reserve(domains.size());
  for (const auto &domain : domains) {
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    auto polygon = Spatial::MakeGeosPolygon(context.Handle, domain.Rings);
    if (!polygon) { return std::unexpected(context.Failure()); }
    const char valid = GEOSisValid_r(context.Handle, polygon.get());
    if (valid == 2) { return std::unexpected(context.Failure()); }
    if (valid == 0) { return std::unexpected(Error::InvalidInput); }
    result.push_back(std::move(polygon));
  }
  return result;
}

Checked<Geometry> Polygonize(const Context &context,
                             std::span<const Geometry> domains,
                             std::span<const PlanarLine> constraints,
                             PlanarArrangementWork &work,
                             PlanarArrangementBudget budget) {
  std::vector<Geometry> lines;
  lines.reserve(domains.size() + constraints.size());
  for (const auto &domain : domains) {
    auto boundary = context.Own(GEOSBoundary_r(context.Handle, domain.get()));
    if (!boundary) { return std::unexpected(context.Failure()); }
    lines.push_back(std::move(boundary));
  }
  for (const auto &constraint : constraints) {
    auto line = MakeLine(context, constraint);
    if (!line) { return std::unexpected(context.Failure()); }
    lines.push_back(std::move(line));
  }
  const auto collection = Collection(context, std::move(lines));
  if (!collection) { return std::unexpected(context.Failure()); }
  const auto network = context.Own(GEOSUnaryUnion_r(context.Handle, collection.get()));
  if (!network) { return std::unexpected(context.Failure()); }
  const int count = GEOSGetNumGeometries_r(context.Handle, network.get());
  if (count < 0) { return std::unexpected(context.Failure()); }
  std::vector<const GEOSGeometry *> parts;
  parts.reserve(static_cast<size_t>(count));
  for (int at = 0; at < count; ++at) {
    const auto *line = GEOSGetGeometryN_r(context.Handle, network.get(), at);
    const int points = GEOSGeomGetNumPoints_r(context.Handle, line);
    if (points < 2) { return std::unexpected(context.Failure()); }
    const auto segments = static_cast<size_t>(points - 1);
    if (segments > budget.NodedSegments - work.NodedSegments) {
      return std::unexpected(Error::CapacityExceeded);
    }
    work.NodedSegments += segments;
    parts.push_back(line);
  }
  auto cells = context.Own(
      GEOSPolygonize_r(context.Handle, parts.data(), static_cast<unsigned int>(parts.size())));
  if (!cells) { return std::unexpected(context.Failure()); }
  return cells;
}

using Triangle = std::array<PlanarPoint, 3>;

Checked<void> ReadTriangles(const Context &context,
                            const GEOSGeometry *triangles,
                            std::vector<Triangle> &output,
                            size_t maximum) {
  const int count = GEOSGetNumGeometries_r(context.Handle, triangles);
  if (count < 0) { return std::unexpected(context.Failure()); }
  if (static_cast<size_t>(count) > maximum - output.size()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  for (int at = 0; at < count; ++at) {
    const auto *triangle = GEOSGetGeometryN_r(context.Handle, triangles, at);
    const auto *ring = GEOSGetExteriorRing_r(context.Handle, triangle);
    const auto *coordinates = GEOSGeom_getCoordSeq_r(context.Handle, ring);
    unsigned int size = 0;
    if (coordinates == nullptr || GEOSCoordSeq_getSize_r(context.Handle, coordinates, &size) == 0 ||
        size != 4) {
      return std::unexpected(context.Failure());
    }
    Triangle result{};
    for (unsigned int corner = 0; corner < 3; ++corner) {
      auto &point = result[corner];
      if (GEOSCoordSeq_getXY_r(context.Handle, coordinates, corner, point.data(), &point[1]) == 0) {
        return std::unexpected(context.Failure());
      }
    }
    output.push_back(result);
  }
  return {};
}

Checked<std::vector<Triangle>> Triangulate(const Context &context,
                                           const GEOSGeometry *cells,
                                           std::span<const Prepared> domains,
                                           PlanarArrangementWork &work,
                                           PlanarArrangementBudget budget) {
  const int count = GEOSGetNumGeometries_r(context.Handle, cells);
  if (count < 0) { return std::unexpected(context.Failure()); }
  work.Cells = static_cast<size_t>(count);
  if (work.Cells > budget.Cells) { return std::unexpected(Error::CapacityExceeded); }
  std::vector<Triangle> raw;
  for (int at = 0; at < count; ++at) {
    const auto *cell = GEOSGetGeometryN_r(context.Handle, cells, at);
    const auto owners = Owners(context, cell, domains, work, budget.DomainTests);
    if (!owners) { return std::unexpected(owners.error()); }
    if (*owners == 0) { continue; }
    const auto triangles =
        context.Own(GEOSConstrainedDelaunayTriangulation_r(context.Handle, cell));
    if (!triangles) { return std::unexpected(context.Failure()); }
    if (auto result = ReadTriangles(context, triangles.get(), raw, budget.Mesh.Faces); !result) {
      return std::unexpected(result.error());
    }
  }
  return raw;
}

Checked<PlanarMesh> IndexAndConform(std::span<const Triangle> raw,
                                    double distance,
                                    PlanarMeshBudget budget,
                                    const std::stop_token &stop) {
  std::map<PlanarPoint, uint32_t> indices;
  for (const auto &face : raw) {
    for (const auto point : face) {
      indices.try_emplace(point, 0);
      if (indices.size() > budget.Vertices ||
          indices.size() > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::CapacityExceeded);
      }
    }
  }
  std::vector<PlanarPoint> points;
  points.reserve(indices.size());
  for (auto &[point, index] : indices) {
    index = static_cast<uint32_t>(points.size());
    points.push_back(point);
  }
  std::vector<PlanarFace> faces;
  faces.reserve(raw.size());
  for (const auto &triangle : raw) {
    PlanarFace face{};
    for (size_t at = 0; at < face.size(); ++at) {
      const auto found = indices.find(triangle[at]);
      assert(found != indices.end());
      face[at] = found->second;
    }
    faces.push_back(face);
  }
  auto mesh = ConformPlanarMesh(points, faces, distance, budget, stop);
  if (mesh) { return std::move(*mesh); }
  switch (mesh.error()) {
    case PlanarMeshError::InvalidInput: return std::unexpected(Error::InvalidInput);
    case PlanarMeshError::CapacityExceeded: return std::unexpected(Error::CapacityExceeded);
    case PlanarMeshError::Canceled: return std::unexpected(Error::Canceled);
    case PlanarMeshError::WeldDistanceExceeded: return std::unexpected(Error::WeldDistanceExceeded);
  }
  return std::unexpected(Error::LibraryFailure);
}
}
}

namespace outshine {
std::expected<PlanarArrangement, PlanarArrangementError>
ArrangePlanarDomains(std::span<const PlanarDomain> domains,
                     std::span<const PlanarLine> constraints,
                     PlanarArrangementRecipe recipe,
                     PlanarArrangementBudget budget,
                     const std::stop_token &stop) {
  using namespace PlanarArrangements;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  PlanarArrangement output;
  if (auto result = Validate(domains, constraints, recipe, budget, output.Work); !result) {
    return std::unexpected(result.error());
  }
  if (domains.empty()) { return output; }
  const Context context(stop);
  if (context.Handle == nullptr) { return std::unexpected(Error::LibraryFailure); }
  const auto polygons = MakeDomains(context, domains);
  if (!polygons) { return std::unexpected(polygons.error()); }
  std::vector<Prepared> prepared;
  prepared.reserve(polygons->size());
  for (const auto &polygon : *polygons) {
    prepared.emplace_back(GEOSPrepare_r(context.Handle, polygon.get()),
                          DestroyPrepared{context.Handle});
    if (!prepared.back()) { return std::unexpected(context.Failure()); }
  }
  const auto cells = Polygonize(context, *polygons, constraints, output.Work, budget);
  if (!cells) { return std::unexpected(cells.error()); }
  const auto triangles = Triangulate(context, cells->get(), prepared, output.Work, budget);
  if (!triangles) { return std::unexpected(triangles.error()); }
  auto mesh = IndexAndConform(*triangles, recipe.WeldDistanceM, budget.Mesh, stop);
  if (!mesh) { return std::unexpected(mesh.error()); }
  output.Mesh = std::move(*mesh);
  if (auto result = AssignAndValidate(context, output, *polygons, prepared, recipe, budget);
      !result) {
    return std::unexpected(result.error());
  }
  return output;
}
}
