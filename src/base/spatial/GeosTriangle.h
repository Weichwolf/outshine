#ifndef OUTSHINE_BASE_SPATIAL_GEOSTRIANGLE_H
#define OUTSHINE_BASE_SPATIAL_GEOSTRIANGLE_H

#include "math/Vec3.h"
#include "GeosGeometry.h"
#include <geos_c.h>
#include <array>

namespace outshine::Spatial {
inline GeosGeometry ProjectedTriangle(GEOSContextHandle_t context,
                                      const std::array<Vec3, 3> &corners) {
  GEOSCoordSequence *coordinates = GEOSCoordSeq_create_r(context, 4, 2);
  if (coordinates == nullptr) { return {nullptr, {context}}; }
  for (unsigned at = 0; at < 4; ++at) {
    if (GEOSCoordSeq_setXY_r(context, coordinates, at, corners[at % 3][0], corners[at % 3][1]) ==
        0) {
      GEOSCoordSeq_destroy_r(context, coordinates);
      return {nullptr, {context}};
    }
  }
  GEOSGeometry *ring = GEOSGeom_createLinearRing_r(context, coordinates);
  if (ring == nullptr) { return {nullptr, {context}}; }
  return {GEOSGeom_createPolygon_r(context, ring, nullptr, 0), {context}};
}
}
#endif
