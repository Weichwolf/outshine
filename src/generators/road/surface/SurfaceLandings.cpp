#include "SurfaceLayoutInternal.h"
#include <geos_c.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceLayouts {
namespace {
constexpr int kQuadrants = 2;
constexpr double kMiterLimit = 5;

Geometry Buffer(const Context &context, const GEOSGeometry *geometry, double radius) {
  return context.Own(GEOSBufferWithStyle_r(context.Handle,
                                           geometry,
                                           radius,
                                           kQuadrants,
                                           GEOSBUF_CAP_ROUND,
                                           GEOSBUF_JOIN_ROUND,
                                           kMiterLimit));
}

struct MaskParts {
  const Context &Geos;
  const GEOSGeometry *Origin;
  std::vector<Geometry> Parts;
};

Checked<void> AppendNearby(const Geometry &intersection, MaskParts &state) {
  const auto &context = state.Geos;
  const int count = GEOSGetNumGeometries_r(context.Handle, intersection.get());
  if (count < 0) { return std::unexpected(context.Failure()); }
  for (int at = 0; at < count; ++at) {
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto *part = GEOSGetGeometryN_r(context.Handle, intersection.get(), at);
    if (part == nullptr) { return std::unexpected(context.Failure()); }
    const char empty = GEOSisEmpty_r(context.Handle, part);
    if (empty == 2) { return std::unexpected(context.Failure()); }
    if (empty == 1) { continue; }
    double distance = 0;
    if (GEOSDistance_r(context.Handle, part, state.Origin, &distance) == 0 ||
        !std::isfinite(distance)) {
      return std::unexpected(context.Failure());
    }
    if (distance >= SurfaceLayoutRecipe::kCoordinateMatchM) { continue; }
    state.Parts.push_back(context.Own(GEOSGeom_clone_r(context.Handle, part)));
    if (!state.Parts.back()) { return std::unexpected(context.Failure()); }
  }
  return {};
}

Checked<Geometry> Mask(const Context &context,
                       const SurfaceLanding &landing,
                       double radius,
                       const Sources &geometries,
                       double reach,
                       SurfaceLayout &output) {
  const auto point = context.Own(
      GEOSGeom_createPointFromXY_r(context.Handle, landing.Origin[0], landing.Origin[1]));
  if (!point) { return std::unexpected(context.Failure()); }
  MaskParts state{.Geos = context, .Origin = point.get(), .Parts = {}};
  state.Parts.push_back(Buffer(context, point.get(), radius));
  if (!state.Parts.back()) { return std::unexpected(context.Failure()); }
  for (size_t first = 0; first < landing.Sources.size(); ++first) {
    for (size_t last = first + 1; last < landing.Sources.size(); ++last) {
      if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      ++output.Work.LandingPairs;
      const auto intersection =
          context.Own(GEOSIntersection_r(context.Handle,
                                         geometries.Domains[landing.Sources[first]].get(),
                                         geometries.Domains[landing.Sources[last]].get()));
      if (!intersection) { return std::unexpected(context.Failure()); }
      const auto nearby = AppendNearby(intersection, state);
      if (!nearby) { return std::unexpected(nearby.error()); }
    }
  }
  std::vector<GEOSGeometry *> raw;
  raw.reserve(state.Parts.size());
  for (auto &part : state.Parts) { raw.push_back(part.release()); }
  const auto collection = context.Own(GEOSGeom_createCollection_r(
      context.Handle, GEOS_GEOMETRYCOLLECTION, raw.data(), static_cast<unsigned>(raw.size())));
  if (!collection) { return std::unexpected(context.Failure()); }
  const auto united = context.Own(GEOSUnaryUnion_r(context.Handle, collection.get()));
  if (!united) { return std::unexpected(context.Failure()); }
  auto result = Buffer(context, united.get(), reach);
  if (!result) { return std::unexpected(context.Failure()); }
  if (GEOSGeomTypeId_r(context.Handle, result.get()) != GEOS_POLYGON) {
    return std::unexpected(Error::DisconnectedLanding);
  }
  return result;
}

Checked<SurfaceHeightPort> Port(const Context &context,
                                const GEOSGeometry *polygon,
                                const SurfaceLanding &landing,
                                SurfaceLayoutBudget budget,
                                SurfaceLayout &output) {
  SurfaceHeightPort port;
  port.Domains.assign(landing.Sources.begin(), landing.Sources.end());
  const int holes = GEOSGetNumInteriorRings_r(context.Handle, polygon);
  if (holes < 0) { return std::unexpected(context.Failure()); }
  for (int ring = -1; ring < holes; ++ring) {
    const auto *boundary = ring == -1 ? GEOSGetExteriorRing_r(context.Handle, polygon)
                                      : GEOSGetInteriorRingN_r(context.Handle, polygon, ring);
    if (boundary == nullptr) { return std::unexpected(context.Failure()); }
    auto points = Coordinates(context, boundary, output, budget);
    if (!points) { return std::unexpected(points.error()); }
    if (points->size() < 4) { return std::unexpected(Error::DisconnectedLanding); }
    if (points->size() > budget.OutputCoordinates - output.Work.OutputCoordinates) {
      return std::unexpected(Error::CapacityExceeded);
    }
    output.Work.OutputCoordinates += points->size();
    output.Constraints.push_back(*points);
    port.Region.Rings.push_back(std::move(*points));
  }
  return port;
}
}

Checked<void> Landings(const Context &context,
                       std::span<const SurfaceLayoutSource> sources,
                       std::span<const SurfaceLanding> landings,
                       const Sources &geometries,
                       SurfaceLayoutRecipe recipe,
                       SurfaceLayoutBudget budget,
                       SurfaceLayout &output) {
  output.Ports.reserve(landings.size());
  for (const auto &landing : landings) {
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (landing.Sources.size() < 2 || !std::isfinite(landing.Origin[0]) ||
        !std::isfinite(landing.Origin[1])) {
      return std::unexpected(Error::InvalidInput);
    }
    uint64_t bits = 0;
    double radius = 0;
    for (const auto source : landing.Sources) {
      if (source >= sources.size() || (bits & (uint64_t{1} << source)) != 0) {
        return std::unexpected(Error::InvalidInput);
      }
      bits |= uint64_t{1} << source;
      radius = std::max(radius, sources[source].WidthM / 2);
    }
    const size_t pairs = landing.Sources.size() * (landing.Sources.size() - 1) / 2;
    if (pairs > budget.LandingPairs - output.Work.LandingPairs) {
      return std::unexpected(Error::CapacityExceeded);
    }
    const auto mask = Mask(context, landing, radius, geometries, recipe.LandingReachM, output);
    if (!mask) { return std::unexpected(mask.error()); }
    auto port = Port(context, mask->get(), landing, budget, output);
    if (!port) { return std::unexpected(port.error()); }
    output.Ports.push_back(std::move(*port));
  }
  return {};
}
}
