#include "RoofSurface.h"
#include "PolygonTriangulation.h"

#include <geos_c.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Generators {
namespace {
struct Context {
  GEOSContextHandle_t Handle = GEOS_init_r();

  ~Context() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }
};

struct DeleteGeometry {
  GEOSContextHandle_t Context;

  void operator()(GEOSGeometry *geometry) const { GEOSGeom_destroy_r(Context, geometry); }
};

using Geometry = std::unique_ptr<GEOSGeometry, DeleteGeometry>;

Geometry Polygon(GEOSContextHandle_t context, std::span<const EastNorth> points) {
  if (points.size() < 3) { return {nullptr, {context}}; }
  auto *coordinates = GEOSCoordSeq_create_r(context, static_cast<unsigned>(points.size() + 1), 2);
  if (coordinates == nullptr) { return {nullptr, {context}}; }
  for (size_t at = 0; at <= points.size(); ++at) {
    const auto p = points[at % points.size()];
    if (GEOSCoordSeq_setXY_r(context, coordinates, static_cast<unsigned>(at), p.EastM, p.NorthM) ==
        0) {
      GEOSCoordSeq_destroy_r(context, coordinates);
      return {nullptr, {context}};
    }
  }
  auto *ring = GEOSGeom_createLinearRing_r(context, coordinates);
  if (ring == nullptr) { return {nullptr, {context}}; }
  return {GEOSGeom_createPolygon_r(context, ring, nullptr, 0), {context}};
}

bool ReadRing(GEOSContextHandle_t context, const GEOSGeometry *ring, PolygonRing &points) {
  const auto *coordinates = GEOSGeom_getCoordSeq_r(context, ring);
  unsigned count = 0;
  if (coordinates == nullptr || GEOSCoordSeq_getSize_r(context, coordinates, &count) == 0 ||
      count < 4) {
    return false;
  }
  points.resize(count - 1);
  for (unsigned at = 0; at + 1 < count; ++at) {
    if (GEOSCoordSeq_getXY_r(context, coordinates, at, points[at].data(), &points[at][1]) == 0) {
      return false;
    }
  }
  return true;
}

bool AppendPolygon(GEOSContextHandle_t context,
                   const GEOSGeometry *polygon,
                   EastNorth origin,
                   std::vector<EastNorth> &triangles) {
  const int holes = GEOSGetNumInteriorRings_r(context, polygon);
  if (holes < 0) { return false; }
  std::vector<PolygonRing> rings(static_cast<size_t>(holes) + 1);
  if (!ReadRing(context, GEOSGetExteriorRing_r(context, polygon), rings[0])) { return false; }
  for (int at = 0; at < holes; ++at) {
    if (!ReadRing(context,
                  GEOSGetInteriorRingN_r(context, polygon, at),
                  rings[static_cast<size_t>(at) + 1])) {
      return false;
    }
  }
  const auto indices = TriangulatePolygon(rings);
  if (!indices) { return false; }
  std::vector<EastNorth> points;
  for (const auto &ring : rings) {
    for (const auto &p : ring) {
      points.push_back({.EastM = p[0] + origin.EastM, .NorthM = p[1] + origin.NorthM});
    }
  }
  for (size_t at = 0; at < indices->size(); at += 3) {
    const auto a = points[(*indices)[at]];
    auto b = points[(*indices)[at + 1]];
    auto c = points[(*indices)[at + 2]];
    const double cross =
        (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
    if (cross < 0) { std::swap(b, c); }
    triangles.insert(triangles.end(), {a, b, c});
  }
  return true;
}

bool AppendIntersection(GEOSContextHandle_t context,
                        const GEOSGeometry *geometry,
                        EastNorth origin,
                        std::vector<EastNorth> &triangles) {
  const char empty = GEOSisEmpty_r(context, geometry);
  if (empty == 1) { return true; }
  if (empty == 2) { return false; }
  const int type = GEOSGeomTypeId_r(context, geometry);
  if (type == GEOS_POLYGON) { return AppendPolygon(context, geometry, origin, triangles); }
  if (type == GEOS_MULTIPOLYGON || type == GEOS_GEOMETRYCOLLECTION) {
    const int count = GEOSGetNumGeometries_r(context, geometry);
    if (count < 0) { return false; }
    for (int at = 0; at < count; ++at) {
      if (!AppendIntersection(
              context, GEOSGetGeometryN_r(context, geometry, at), origin, triangles)) {
        return false;
      }
    }
  }
  return type >= 0;
}
}

bool RoofSurface::CoverHip(std::span<const EastNorth> requested,
                           BuildingScratch &scratch,
                           std::vector<EastNorth> &triangles) const {
  if (Hip_ == nullptr || !*Hip_) { return Fill(requested, scratch, triangles, Shape_.Holes); }
  const auto &plan = **Hip_;
  const auto origin = plan.Origin();
  const size_t first = triangles.size();
  const bool complete = std::ranges::equal(requested, Shape_.Ring);
  const Context context;
  if (context.Handle == nullptr) { return false; }
  Geometry clip{nullptr, {context.Handle}};
  if (!complete) {
    std::vector<EastNorth> local;
    local.reserve(requested.size());
    for (const auto p : requested) {
      local.push_back({.EastM = p.EastM - origin.EastM, .NorthM = p.NorthM - origin.NorthM});
    }
    clip = Polygon(context.Handle, local);
    if (!clip) { return false; }
  }
  for (const auto &facet : plan.Facets()) {
    auto polygon = Polygon(context.Handle, facet.Ring);
    if (!polygon) {
      triangles.resize(first);
      return false;
    }
    if (!complete) {
      polygon.reset(GEOSIntersection_r(context.Handle, polygon.get(), clip.get()));
      if (!polygon) {
        triangles.resize(first);
        return false;
      }
    }
    if (!AppendIntersection(context.Handle, polygon.get(), origin, triangles)) {
      triangles.resize(first);
      return false;
    }
  }
  return true;
}

}
