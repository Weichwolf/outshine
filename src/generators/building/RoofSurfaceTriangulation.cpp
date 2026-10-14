#include "RoofSurface.h"
#include "PolygonTriangulation.h"

#include <geos_c.h>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
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

Geometry LinearRing(GEOSContextHandle_t context, std::span<const EastNorth> points) {
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
  return {GEOSGeom_createLinearRing_r(context, coordinates), {context}};
}

Geometry Polygon(GEOSContextHandle_t context,
                 std::span<const EastNorth> points,
                 std::span<const std::vector<EastNorth>> holes = {}) {
  auto exterior = LinearRing(context, points);
  if (!exterior) { return {nullptr, {context}}; }
  std::vector<Geometry> owned;
  owned.reserve(holes.size());
  for (const auto &hole : holes) {
    owned.push_back(LinearRing(context, hole));
    if (!owned.back()) { return {nullptr, {context}}; }
  }
  std::vector<GEOSGeometry *> inner;
  inner.reserve(owned.size());
  for (auto &hole : owned) { inner.push_back(hole.release()); }
  return {GEOSGeom_createPolygon_r(
              context, exterior.release(), inner.data(), static_cast<unsigned>(inner.size())),
          {context}};
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
  double areaM2 = 0;
  constexpr double kWeldAreaM2 = 1 / (kBuildingWeldPerM * kBuildingWeldPerM);
  if (GEOSArea_r(context, polygon, &areaM2) == 0) { return false; }
  if (areaM2 <= kWeldAreaM2) { return true; }
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

struct QuadInput {
  std::span<const EastNorth> Requested, Local;
};

bool AppendQuad(QuadInput quad, std::vector<EastNorth> &triangles) {
  const auto requested = quad.Requested;
  const auto local = quad.Local;
  if (requested.size() != 4) { return false; }
  const auto cross = [&](size_t a, size_t b, size_t c) {
    return (local[b].EastM - local[a].EastM) * (local[c].NorthM - local[a].NorthM) -
           (local[b].NorthM - local[a].NorthM) * (local[c].EastM - local[a].EastM);
  };
  if (cross(3, 0, 1) <= 0 || cross(3, 1, 2) <= 0) { return false; }
  triangles.reserve(triangles.size() + 6);
  for (const size_t at : {3u, 0u, 1u, 3u, 1u, 2u}) { triangles.push_back(requested[at]); }
  return true;
}

std::optional<std::vector<std::vector<EastNorth>>>
LocalHoles(std::span<const std::vector<EastNorth>> holes, EastNorth origin) {
  std::vector<std::vector<EastNorth>> local;
  local.reserve(holes.size());
  for (const auto &hole : holes) {
    auto &ring = local.emplace_back();
    ring.reserve(hole.size());
    for (const auto p : hole) {
      if (!std::isfinite(p.EastM) || !std::isfinite(p.NorthM)) { return std::nullopt; }
      ring.push_back({.EastM = p.EastM - origin.EastM, .NorthM = p.NorthM - origin.NorthM});
    }
  }
  return local;
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

bool RoofSurface::Fill(std::span<const EastNorth> requested,
                       BuildingScratch &scratch,
                       std::vector<EastNorth> &triangles,
                       std::span<const std::vector<EastNorth>> holes) {
  (void)scratch;
  if (requested.size() < 3) { return false; }
  const auto origin = requested.front();
  std::vector<EastNorth> local;
  double twiceArea = 0;
  for (size_t at = 0; at < requested.size(); ++at) {
    const auto a = requested[at];
    const auto b = requested[(at + 1) % requested.size()];
    if (!std::isfinite(a.EastM) || !std::isfinite(a.NorthM)) { return false; }
    const EastNorth p{.EastM = a.EastM - origin.EastM, .NorthM = a.NorthM - origin.NorthM};
    local.push_back(p);
    twiceArea += p.EastM * (b.NorthM - origin.NorthM) - p.NorthM * (b.EastM - origin.EastM);
  }
  if (!std::isfinite(twiceArea) || (holes.empty() && twiceArea <= 0)) { return false; }
  if (holes.empty() && AppendQuad({.Requested = requested, .Local = local}, triangles)) {
    return true;
  }
  const auto localHoles = LocalHoles(holes, origin);
  if (!localHoles) { return false; }
  const Context context;
  if (context.Handle == nullptr) { return false; }
  auto polygon = Polygon(context.Handle, local, *localHoles);
  if (!polygon) { return false; }
  const char valid = GEOSisValid_r(context.Handle, polygon.get());
  if (valid == 2) { return false; }
  if (valid == 0) {
    if (!holes.empty()) { return false; }
    polygon.reset(GEOSMakeValid_r(context.Handle, polygon.get()));
  }
  if (!polygon) { return false; }
  const size_t first = triangles.size();
  if (!AppendIntersection(context.Handle, polygon.get(), origin, triangles) ||
      triangles.size() == first) {
    triangles.resize(first);
    return false;
  }
  return true;
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
