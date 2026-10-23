#include "GeosPolygon.h"
#include <geos_c.h>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Spatial {
namespace {
struct DestroyCoordinates {
  GEOSContextHandle_t Context;

  void operator()(GEOSCoordSequence *sequence) const { GEOSCoordSeq_destroy_r(Context, sequence); }
};

GeosGeometry MakeRing(GEOSContextHandle_t context, const PolygonRing &ring) {
  const bool closed = ring.front() == ring.back();
  const auto count = static_cast<unsigned int>(ring.size() + (closed ? 0u : 1u));
  std::unique_ptr<GEOSCoordSequence, DestroyCoordinates> coordinates(
      GEOSCoordSeq_create_r(context, count, 2), DestroyCoordinates{context});
  if (!coordinates) { return {nullptr, {context}}; }
  for (unsigned int at = 0; at < count; ++at) {
    const auto &point = ring[at % ring.size()];
    if (GEOSCoordSeq_setXY_r(context, coordinates.get(), at, point[0], point[1]) == 0) {
      return {nullptr, {context}};
    }
  }
  return {GEOSGeom_createLinearRing_r(context, coordinates.release()), {context}};
}
}

GeosGeometry MakeGeosPolygon(GEOSContextHandle_t context, std::span<const PolygonRing> rings) {
  GeosGeometry shell = MakeRing(context, rings.front());
  if (!shell) { return shell; }
  std::vector<GeosGeometry> holes;
  for (const auto &ring : rings.subspan(1)) {
    holes.push_back(MakeRing(context, ring));
    if (!holes.back()) { return {nullptr, {context}}; }
  }
  std::vector<GEOSGeometry *> inner;
  inner.reserve(holes.size());
  for (auto &hole : holes) { inner.push_back(hole.release()); }
  return {GEOSGeom_createPolygon_r(
              context, shell.release(), inner.data(), static_cast<unsigned int>(inner.size())),
          {context}};
}
}
