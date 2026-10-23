#include "UrbanAreaDomain.h"
#include "Wgs84.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm::UrbanDetail {
namespace {
Spatial::GeosGeometry
Coordinates(const Domain &domain, const OsmField &field, GeographicRing ring, bool closed) {
  if (ring.Count < (closed ? 3u : 2u)) { return domain.Own(nullptr); }
  const auto points = field.Points();
  const size_t first = static_cast<size_t>(ring.First) * 2;
  const size_t last = first + static_cast<size_t>(ring.Count - 1) * 2;
  const bool repeated = points[first] == points[last] && points[first + 1] == points[last + 1];
  const uint32_t count = ring.Count + (closed && !repeated ? 1u : 0u);
  auto *coordinates = GEOSCoordSeq_create_r(domain.Context, count, 2);
  if (coordinates == nullptr) { return domain.Own(nullptr); }
  constexpr double radians = std::numbers::pi / 180;
  for (uint32_t i = 0; i < count; ++i) {
    const size_t at = first + static_cast<size_t>(i % ring.Count) * 2;
    const double x = kWgs84A * (points[at + 1] * radians);
    const double y = kWgs84A * std::asinh(std::tan(points[at] * radians));
    if (GEOSCoordSeq_setXY_r(domain.Context, coordinates, i, x, y) == 0) {
      GEOSCoordSeq_destroy_r(domain.Context, coordinates);
      return domain.Own(nullptr);
    }
  }
  return domain.Own(closed ? GEOSGeom_createLinearRing_r(domain.Context, coordinates)
                           : GEOSGeom_createLineString_r(domain.Context, coordinates));
}

Spatial::GeosGeometry
Polygon(const Domain &domain, const OsmField &field, std::span<const GeographicRing> rings) {
  auto shell = Coordinates(domain, field, rings.front(), true);
  if (!shell) { return shell; }
  std::vector<Spatial::GeosGeometry> holes;
  for (const auto ring : rings.subspan(1)) {
    holes.push_back(Coordinates(domain, field, ring, true));
    if (!holes.back()) { return domain.Own(nullptr); }
  }
  std::vector<GEOSGeometry *> raw;
  raw.reserve(holes.size());
  for (auto &hole : holes) { raw.push_back(hole.release()); }
  return domain.Own(GEOSGeom_createPolygon_r(
      domain.Context, shell.release(), raw.data(), static_cast<unsigned>(raw.size())));
}
}

Spatial::GeosGeometry
Collect(const Domain &domain, std::vector<Spatial::GeosGeometry> parts, int type) {
  std::vector<GEOSGeometry *> raw;
  raw.reserve(parts.size());
  for (const auto &part : parts) {
    if (!part) { return domain.Own(nullptr); }
  }
  for (auto &part : parts) { raw.push_back(part.release()); }
  return domain.Own(GEOSGeom_createCollection_r(
      domain.Context, type, raw.data(), static_cast<unsigned>(raw.size())));
}

Spatial::GeosGeometry
SourceGeometry(const Domain &domain, const OsmField &field, const OsmField::Feature &feature) {
  const auto rings = field.Rings().subspan(feature.FirstRing, feature.RingCount);
  if (rings.empty() || (feature.Type != 2 && feature.Type != 3)) { return domain.Own(nullptr); }
  std::vector<Spatial::GeosGeometry> parts;
  for (size_t first = 0; first < rings.size();) {
    if (feature.Type == 2) {
      parts.push_back(Coordinates(domain, field, rings[first++], false));
    } else {
      size_t last = first + 1;
      while (last < rings.size() && !rings[last].Exterior) { ++last; }
      parts.push_back(Polygon(domain, field, rings.subspan(first, last - first)));
      first = last;
    }
    if (!parts.back()) { return domain.Own(nullptr); }
  }
  return Collect(
      domain, std::move(parts), feature.Type == 3 ? GEOS_MULTIPOLYGON : GEOS_MULTILINESTRING);
}
}
