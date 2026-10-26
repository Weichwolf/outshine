#ifndef OUTSHINE_BASE_SPATIAL_PLANARREGIONINTERNAL_H
#define OUTSHINE_BASE_SPATIAL_PLANARREGIONINTERNAL_H

#include "GeosPolygon.h"
#include "PlanarRegionRemesh.h"
#include <map>
#include <set>

namespace outshine::Spatial::Regions {
using Error = PlanarRegionError;
template <typename T> using Checked = std::expected<T, Error>;
using PointIndex = std::map<PlanarPoint, uint32_t>;

struct State {
  GEOSContextHandle_t Context = GEOS_init_r();
  std::span<const PlanarPoint> Points;
  std::span<const PlanarFace> Faces;
  PlanarRegionBudget Budget;
  const std::stop_token &Stop;
  std::set<PlanarPoint> Protected;
  PlanarRegionMesh Output;
  size_t UnselectedFaces = 0;

  ~State() {
    if (Context != nullptr) { GEOS_finish_r(Context); }
  }

  [[nodiscard]] GeosGeometry Own(GEOSGeometry *geometry) const {
    return {geometry, DestroyGeosGeometry{Context}};
  }

  [[nodiscard]] Checked<void> Check() const {
    if (Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    return {};
  }
};

[[nodiscard]] Checked<PolygonRing> CleanRing(State &state, const GEOSGeometry *geometry);
[[nodiscard]] Checked<void> TriangulateRegion(State &state,
                                              const GEOSGeometry *shape,
                                              const PointIndex &lookup,
                                              std::vector<PlanarFace> &faces);
}
#endif
