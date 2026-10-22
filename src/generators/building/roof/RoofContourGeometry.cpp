#include "RoofContourInternal.h"
#include <expected>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
Geometry Line(const Geos &g, std::span<const Point> points, bool ring) {
  if (points.empty()) { return g.Own(nullptr); }
  const size_t count = points.size() + (ring ? 1 : 0);
  auto *coordinates = GEOSCoordSeq_create_r(g.Handle, static_cast<unsigned int>(count), 2);
  if (coordinates == nullptr) { return g.Own(nullptr); }
  for (size_t at = 0; at < count; ++at) {
    if (GEOSCoordSeq_setXY_r(g.Handle,
                             coordinates,
                             static_cast<unsigned int>(at),
                             points[at % points.size()][0],
                             points[at % points.size()][1]) == 0) {
      GEOSCoordSeq_destroy_r(g.Handle, coordinates);
      return g.Own(nullptr);
    }
  }
  return g.Own(ring ? GEOSGeom_createLinearRing_r(g.Handle, coordinates)
                    : GEOSGeom_createLineString_r(g.Handle, coordinates));
}

Geometry Collection(const Geos &g, std::vector<Geometry> parts, int type) {
  if (parts.empty()) { return g.Own(GEOSGeom_createEmptyCollection_r(g.Handle, type)); }
  std::vector<GEOSGeometry *> raw;
  raw.reserve(parts.size());
  for (const auto &part : parts) {
    if (!part) { return g.Own(nullptr); }
  }
  for (auto &part : parts) { raw.push_back(part.release()); }
  return g.Own(GEOSGeom_createCollection_r(
      g.Handle, type, raw.data(), static_cast<unsigned int>(raw.size())));
}

Checked<std::vector<Point>> Coordinates(const Geos &g, const GEOSGeometry *geometry, bool closed) {
  if (geometry == nullptr) { return std::unexpected(Failure{"missing coordinates"}); }
  const auto *sequence = GEOSGeom_getCoordSeq_r(g.Handle, geometry);
  unsigned int count = 0;
  if ((sequence == nullptr) || (GEOSCoordSeq_getSize_r(g.Handle, sequence, &count) == 0) ||
      count > kMaximumWeldVertices) {
    return std::unexpected(Failure{HipRoofError::CapacityExceeded, "coordinate decode/budget"});
  }
  std::vector<Point> result;
  result.reserve(count);
  for (unsigned int at = 0; at < count; ++at) {
    Point p{};
    if (GEOSCoordSeq_getXY_r(g.Handle, sequence, at, p.data(), &p[1]) == 0) {
      return std::unexpected(Failure{"coordinate decode"});
    }
    result.push_back(p);
  }
  if (closed && !result.empty()) {
    if (result.front() != result.back()) { return std::unexpected(Failure{"unclosed ring"}); }
    result.pop_back();
  }
  return result;
}

Geometry Polygon(const Geos &g, std::span<const Point> points) {
  auto shell = Line(g, points, true);
  if (!shell) { return g.Own(nullptr); }
  return g.Own(GEOSGeom_createPolygon_r(g.Handle, shell.release(), nullptr, 0));
}

Geometry Footprint(const Geos &g, std::span<const std::vector<Point>> rings) {
  if (rings.empty()) { return g.Own(nullptr); }
  auto shell = Line(g, rings.front(), true);
  if (!shell) { return g.Own(nullptr); }
  std::vector<Geometry> holes;
  std::vector<GEOSGeometry *> raw;
  for (const auto &ring : rings.subspan(1)) {
    holes.push_back(Line(g, ring, true));
    if (!holes.back()) { return g.Own(nullptr); }
  }
  raw.reserve(holes.size());
  for (auto &hole : holes) { raw.push_back(hole.release()); }
  return g.Own(GEOSGeom_createPolygon_r(
      g.Handle, shell.release(), raw.data(), static_cast<unsigned int>(raw.size())));
}
}
