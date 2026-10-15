#ifndef OUTSHINE_BASE_SPATIAL_GEOSGEOMETRY_H
#define OUTSHINE_BASE_SPATIAL_GEOSGEOMETRY_H

#include <geos_c.h>
#include <memory>

namespace outshine::Spatial {
struct DestroyGeosGeometry {
  GEOSContextHandle_t Context;

  void operator()(GEOSGeometry *geometry) const { GEOSGeom_destroy_r(Context, geometry); }
};

using GeosGeometry = std::unique_ptr<GEOSGeometry, DestroyGeosGeometry>;
}
#endif
