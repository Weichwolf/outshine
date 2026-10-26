#ifndef OUTSHINE_BASE_SPATIAL_GEOSPOLYGON_H
#define OUTSHINE_BASE_SPATIAL_GEOSPOLYGON_H

#include "GeosGeometry.h"
#include "PolygonTriangulation.h"
#include <span>

namespace outshine::Spatial {
[[nodiscard]] GeosGeometry MakeGeosPolygon(GEOSContextHandle_t context,
                                           std::span<const PolygonRing> rings);
}
#endif
