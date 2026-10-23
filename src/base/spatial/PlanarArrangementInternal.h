#ifndef OUTSHINE_BASE_SPATIAL_PLANARARRANGEMENTINTERNAL_H
#define OUTSHINE_BASE_SPATIAL_PLANARARRANGEMENTINTERNAL_H

#include "PlanarArrangement.h"
#include "GeosGeometry.h"
#include <geos_c.h>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::PlanarArrangements {
using Error = PlanarArrangementError;
using Geometry = Spatial::GeosGeometry;
template <typename T> using Checked = std::expected<T, Error>;

struct DestroyPrepared {
  GEOSContextHandle_t Context;

  void operator()(const GEOSPreparedGeometry *geometry) const {
    GEOSPreparedGeom_destroy_r(Context, geometry);
  }
};

using Prepared = std::unique_ptr<const GEOSPreparedGeometry, DestroyPrepared>;

struct Context {
  GEOSContextHandle_t Handle = GEOS_init_r();
  const std::stop_token &Stop;
  explicit Context(const std::stop_token &stop);
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;
  ~Context();

  [[nodiscard]] Geometry Own(GEOSGeometry *geometry) const { return {geometry, {Handle}}; }

  [[nodiscard]] Error Failure() const {
    return Stop.stop_requested() ? Error::Canceled : Error::LibraryFailure;
  }
};

[[nodiscard]] Geometry Collection(const Context &context, std::vector<Geometry> parts);
[[nodiscard]] Checked<uint64_t> Owners(const Context &context,
                                       const GEOSGeometry *shape,
                                       std::span<const Prepared> domains,
                                       PlanarArrangementWork &work,
                                       size_t maximumTests);
[[nodiscard]] Checked<void> AssignAndValidate(const Context &context,
                                              PlanarArrangement &arrangement,
                                              std::span<const Geometry> domains,
                                              std::span<const Prepared> prepared,
                                              PlanarArrangementRecipe recipe,
                                              PlanarArrangementBudget budget);
}
#endif
