#include "PlanarRegionInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>
#include <vector>

namespace outshine::Spatial::Regions {
namespace {
Checked<PlanarFace>
ReadFace(const State &state, const GEOSGeometry *part, const PointIndex &lookup) {
  const auto *ring = part != nullptr ? GEOSGetExteriorRing_r(state.Context, part) : nullptr;
  const auto *coordinates = ring != nullptr ? GEOSGeom_getCoordSeq_r(state.Context, ring) : nullptr;
  unsigned int size = 0;
  if (coordinates == nullptr || GEOSCoordSeq_getSize_r(state.Context, coordinates, &size) == 0 ||
      size != 4) {
    return std::unexpected(Error::LibraryFailure);
  }
  PlanarFace face{};
  for (uint32_t corner = 0; corner < face.size(); ++corner) {
    PlanarPoint point{};
    if (GEOSCoordSeq_getXY_r(state.Context, coordinates, corner, point.data(), &point[1]) == 0) {
      return std::unexpected(Error::LibraryFailure);
    }
    const auto found = lookup.find(point);
    if (found == lookup.end()) { return std::unexpected(Error::AmbiguousVertices); }
    face[corner] = found->second;
  }
  const auto a = state.Points[face[0]];
  const auto b = state.Points[face[1]];
  const auto c = state.Points[face[2]];
  const double area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  if (!std::isfinite(area) || area == 0) { return std::unexpected(Error::CoverageChanged); }
  if (area < 0) { std::ranges::reverse(face); }
  return face;
}

Checked<void> ReadTriangles(State &state,
                            const GEOSGeometry *triangles,
                            const PointIndex &lookup,
                            std::vector<PlanarFace> &output) {
  const int count = GEOSGetNumGeometries_r(state.Context, triangles);
  if (count <= 0) { return std::unexpected(Error::LibraryFailure); }
  if (static_cast<size_t>(count) >
      state.Budget.Faces - state.UnselectedFaces - state.Output.Work.OutputFaces) {
    return std::unexpected(Error::CapacityExceeded);
  }
  output.reserve(output.size() + static_cast<size_t>(count));
  for (int at = 0; at < count; ++at) {
    if (auto valid = state.Check(); !valid) { return valid; }
    const auto face = ReadFace(state, GEOSGetGeometryN_r(state.Context, triangles, at), lookup);
    if (!face) { return std::unexpected(face.error()); }
    output.push_back(*face);
    ++state.Output.Work.OutputFaces;
  }
  return {};
}

Checked<GeosGeometry> CleanPolygon(State &state, const GEOSGeometry *polygon) {
  const int holes = GEOSGetNumInteriorRings_r(state.Context, polygon);
  if (holes < 0) { return std::unexpected(Error::LibraryFailure); }
  std::vector<PolygonRing> rings;
  rings.reserve(static_cast<size_t>(holes) + 1);
  for (int at = -1; at < holes; ++at) {
    const auto *ring = at < 0 ? GEOSGetExteriorRing_r(state.Context, polygon)
                              : GEOSGetInteriorRingN_r(state.Context, polygon, at);
    if (ring == nullptr) { return std::unexpected(Error::LibraryFailure); }
    auto result = CleanRing(state, ring);
    if (!result) { return std::unexpected(result.error()); }
    rings.push_back(std::move(*result));
  }
  auto result = MakeGeosPolygon(state.Context, rings);
  if (!result) { return std::unexpected(Error::LibraryFailure); }
  const char valid = GEOSisValid_r(state.Context, result.get());
  if (valid == 2) { return std::unexpected(Error::LibraryFailure); }
  if (valid != 1 || GEOSGetNumInteriorRings_r(state.Context, result.get()) != holes) {
    return std::unexpected(Error::CoverageChanged);
  }
  return result;
}
}

Checked<void> TriangulateRegion(State &state,
                                const GEOSGeometry *shape,
                                const PointIndex &lookup,
                                std::vector<PlanarFace> &faces) {
  const int count = GEOSGetNumGeometries_r(state.Context, shape);
  if (count < 0) { return std::unexpected(Error::LibraryFailure); }
  double areaError = 0;
  for (int at = 0; at < count; ++at) {
    if (auto valid = state.Check(); !valid) { return valid; }
    const auto *polygon = GEOSGetGeometryN_r(state.Context, shape, at);
    if (polygon == nullptr || GEOSGeomTypeId_r(state.Context, polygon) != GEOS_POLYGON) {
      return std::unexpected(Error::InvalidInput);
    }
    auto result = CleanPolygon(state, polygon);
    if (!result) { return std::unexpected(result.error()); }
    const auto difference = state.Own(GEOSSymDifference_r(state.Context, polygon, result->get()));
    double area = 0;
    if (!difference || GEOSArea_r(state.Context, difference.get(), &area) == 0) {
      return std::unexpected(Error::LibraryFailure);
    }
    areaError += area;
    if (!std::isfinite(areaError) || areaError > state.Budget.CoverageM2) {
      return std::unexpected(Error::CoverageChanged);
    }
    const auto triangles =
        state.Own(GEOSConstrainedDelaunayTriangulation_r(state.Context, result->get()));
    if (!triangles) { return std::unexpected(Error::LibraryFailure); }
    if (auto read = ReadTriangles(state, triangles.get(), lookup, faces); !read) { return read; }
  }
  state.Output.Work.CoverageErrorM2 += areaError;
  return {};
}
}
