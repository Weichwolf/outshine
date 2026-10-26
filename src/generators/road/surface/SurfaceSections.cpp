#include "SurfaceLayoutInternal.h"
#include <geos_c.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Generators::SurfaceLayouts {
namespace {
Checked<PlanarPoint> At(const Context &context, const GEOSGeometry *axis, double station) {
  const auto point = context.Own(GEOSInterpolate_r(context.Handle, axis, station));
  PlanarPoint result{};
  if (!point || GEOSGeomGetX_r(context.Handle, point.get(), result.data()) == 0 ||
      GEOSGeomGetY_r(context.Handle, point.get(), &result[1]) == 0 || !std::isfinite(result[0]) ||
      !std::isfinite(result[1])) {
    return std::unexpected(context.Failure());
  }
  return result;
}

struct SliceQuery {
  const GEOSGeometry *Axis;
  const GEOSGeometry *Domain;
  double StationM = 0;
  double WidthM = 0;
  double DeltaM = 0;
};

Checked<Geometry> Slice(const Context &context, const SliceQuery &query) {
  const auto point = At(context, query.Axis, query.StationM);
  const auto first = At(context, query.Axis, query.StationM - query.DeltaM);
  const auto last = At(context, query.Axis, query.StationM + query.DeltaM);
  if (!point || !first || !last) { return std::unexpected(context.Failure()); }
  const PlanarPoint tangent{(*last)[0] - (*first)[0], (*last)[1] - (*first)[1]};
  const double length = std::hypot(tangent[0], tangent[1]);
  if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
  const PlanarPoint normal{-tangent[1] / length * (2 * query.WidthM),
                           tangent[0] / length * (2 * query.WidthM)};
  const std::array<PlanarPoint, 2> endpoints{{{(*point)[0] - normal[0], (*point)[1] - normal[1]},
                                              {(*point)[0] + normal[0], (*point)[1] + normal[1]}}};
  if (endpoints[0] == endpoints[1]) { return std::unexpected(Error::InvalidInput); }
  const auto line = Line(context, endpoints);
  if (!line) { return std::unexpected(context.Failure()); }
  auto intersection = context.Own(GEOSIntersection_r(context.Handle, query.Domain, line.get()));
  if (!intersection) { return std::unexpected(context.Failure()); }
  return intersection;
}

Checked<void> Lines(const Context &context,
                    const GEOSGeometry *geometry,
                    SurfaceLayoutBudget budget,
                    SurfaceLayout &output) {
  const int count = GEOSGetNumGeometries_r(context.Handle, geometry);
  if (count < 0) { return std::unexpected(context.Failure()); }
  for (int at = 0; at < count; ++at) {
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto *part = GEOSGetGeometryN_r(context.Handle, geometry, at);
    if (part == nullptr) { return std::unexpected(context.Failure()); }
    const int type = GEOSGeomTypeId_r(context.Handle, part);
    if (type == GEOS_POINT) { continue; }
    if (type != GEOS_LINESTRING) { return std::unexpected(Error::InvalidInput); }
    const char empty = GEOSisEmpty_r(context.Handle, part);
    if (empty == 2) { return std::unexpected(context.Failure()); }
    if (empty == 1) { continue; }
    if (output.Work.SectionParts == budget.SectionParts) {
      return std::unexpected(Error::CapacityExceeded);
    }
    auto points = Coordinates(context, part, output, budget);
    if (!points) { return std::unexpected(points.error()); }
    if (points->size() < 2 || points->front() == points->back()) {
      return std::unexpected(Error::InvalidInput);
    }
    output.Constraints.push_back(std::move(*points));
    ++output.Work.SectionParts;
  }
  return {};
}
}

Checked<void> Sections(const Context &context,
                       std::span<const SurfaceLayoutSource> sources,
                       const Sources &geometries,
                       SurfaceLayoutRecipe recipe,
                       SurfaceLayoutBudget budget,
                       SurfaceLayout &output) {
  constexpr double kMinimumSpacingM = .25;
  constexpr double kMaximumSpacingM = 10;
  constexpr double kMaximumTangentDeltaM = 1e-4;
  if (recipe.SectionSpacingM &&
      (!std::isfinite(*recipe.SectionSpacingM) || *recipe.SectionSpacingM < kMinimumSpacingM ||
       *recipe.SectionSpacingM > kMaximumSpacingM)) {
    return std::unexpected(Error::InvalidInput);
  }
  output.Sections.resize(sources.size());
  for (size_t source = 0; source < sources.size(); ++source) {
    auto &range = output.Sections[source];
    range.First = static_cast<uint32_t>(output.Constraints.size());
    if (!recipe.SectionSpacingM || sources[source].Steps) { continue; }
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    double length = 0;
    if (GEOSLength_r(context.Handle, geometries.Axes[source].get(), &length) == 0) {
      return std::unexpected(context.Failure());
    }
    const double spacing = *recipe.SectionSpacingM;
    const double limit = length - spacing / 4;
    const double needed = std::max(0.0, std::ceil(limit / spacing) - 1);
    if (!std::isfinite(needed) || needed > static_cast<double>(budget.SectionsPerSource)) {
      return std::unexpected(Error::CapacityExceeded);
    }
    const double delta = std::min(kMaximumTangentDeltaM, spacing / 16);
    for (size_t sample = 0; sample < static_cast<size_t>(needed); ++sample) {
      if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      const double station = spacing + static_cast<double>(sample) * spacing;
      const auto slice = Slice(context,
                               {.Axis = geometries.Axes[source].get(),
                                .Domain = geometries.Domains[source].get(),
                                .StationM = station,
                                .WidthM = sources[source].WidthM,
                                .DeltaM = delta});
      if (!slice) { return std::unexpected(slice.error()); }
      ++output.Work.SectionQueries;
      const auto lines = Lines(context, slice->get(), budget, output);
      if (!lines) { return std::unexpected(lines.error()); }
    }
    range.Count = static_cast<uint32_t>(output.Constraints.size() - range.First);
  }
  return {};
}
}
