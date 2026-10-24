#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACELAYOUTINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACELAYOUTINTERNAL_H

#include "SurfaceLayout.h"
#include "GeosGeometry.h"
#include <geos_c.h>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators::SurfaceLayouts {
using Error = SurfaceLayoutError;
using Geometry = Spatial::GeosGeometry;
template <typename T> using Checked = std::expected<T, Error>;

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

struct Sources {
  std::vector<Geometry> Domains;
  std::vector<Geometry> Axes;
};

[[nodiscard]] Geometry Line(const Context &context, std::span<const PlanarPoint> points);
[[nodiscard]] Checked<PlanarLine> Coordinates(const Context &context,
                                              const GEOSGeometry *line,
                                              SurfaceLayout &output,
                                              SurfaceLayoutBudget budget);
[[nodiscard]] Checked<void> Landings(const Context &context,
                                     std::span<const SurfaceLayoutSource> sources,
                                     std::span<const SurfaceLanding> landings,
                                     const Sources &geometries,
                                     SurfaceLayoutRecipe recipe,
                                     SurfaceLayoutBudget budget,
                                     SurfaceLayout &output);
[[nodiscard]] Checked<void> Sections(const Context &context,
                                     std::span<const SurfaceLayoutSource> sources,
                                     const Sources &geometries,
                                     SurfaceLayoutRecipe recipe,
                                     SurfaceLayoutBudget budget,
                                     SurfaceLayout &output);
[[nodiscard]] Checked<std::vector<PlanarLine>>
NodingLines(const SurfaceLayout &output, double distance, const std::stop_token &stop);
[[nodiscard]] Checked<void> ValidateSections(SurfaceLayout &output,
                                             double distance,
                                             SurfaceLayoutBudget budget,
                                             const std::stop_token &stop);
}
#endif
