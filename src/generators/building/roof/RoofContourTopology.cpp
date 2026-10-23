#include "RoofContourInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
struct OwnershipView {
  std::span<const Segment> Segments;
  std::span<const Point> Normals;
  std::span<const double> Intercepts;
  std::span<const Geometry> Sources;
  Step At;
};

bool EligibleOwner(const OwnershipView &query, const Segment &segment, std::array<Point, 2> edge) {
  const auto owner = segment.Owner;
  const double guard = query.At.ToleranceM * 8;
  return std::abs(Dot(edge[0], query.Normals[owner]) + query.Intercepts[owner] - query.At.NowM) <=
             guard &&
         std::abs(Dot(edge[1], query.Normals[owner]) + query.Intercepts[owner] - query.At.NowM) <=
             guard &&
         Dot(Subtract(edge[1], edge[0]), Subtract(segment.To, segment.From)) > 0;
}

Checked<bool> WithinSegment(const Geos &g,
                            std::array<const GEOSGeometry *, 3> sites,
                            const GEOSGeometry *source,
                            double tolerance) {
  std::array<double, 3> distances{};
  for (size_t at = 0; at < sites.size(); ++at) {
    if (GEOSDistance_r(g.Handle, sites[at], source, &distances[at]) == 0) {
      return std::unexpected(Failure{"ownership distance"});
    }
  }
  return std::ranges::all_of(distances, [tolerance](double distance) {
    return std::isfinite(distance) && distance <= tolerance;
  });
}

Checked<size_t> EdgeOwner(const Geos &g, const OwnershipView &query, std::array<Point, 2> edge) {
  const auto a = edge[0];
  const auto b = edge[1];
  const auto start = g.Own(GEOSGeom_createPointFromXY_r(g.Handle, a[0], a[1]));
  const auto end = g.Own(GEOSGeom_createPointFromXY_r(g.Handle, b[0], b[1]));
  const auto midpoint =
      g.Own(GEOSGeom_createPointFromXY_r(g.Handle, (a[0] + b[0]) * .5, (a[1] + b[1]) * .5));
  if (!start || !end || !midpoint) { return std::unexpected(Failure{"ownership points"}); }
  std::optional<size_t> owner;
  for (size_t at = 0; at < query.Segments.size(); ++at) {
    const auto &segment = query.Segments[at];
    if (!EligibleOwner(query, segment, edge)) { continue; }
    const auto within = WithinSegment(
        g, {midpoint.get(), start.get(), end.get()}, query.Sources[at].get(), query.At.ToleranceM);
    if (!within) { return std::unexpected(within.error()); }
    if (!*within) { continue; }
    if (owner && *owner != segment.Owner) {
      return std::unexpected(Failure{"ambiguous source provenance"});
    }
    owner = segment.Owner;
  }
  if (!owner) { return std::unexpected(Failure{"no source provenance"}); }
  return *owner;
}

Checked<Loop> BoundLoop(const Geos &g,
                        const GEOSGeometry *boundary,
                        const ContourGeometry &geometry,
                        const ContourBoundary &source,
                        const OwnershipView &query) {
  const auto snapped =
      g.Own(GEOSSnap_r(g.Handle, boundary, geometry.Anchors.get(), query.At.ToleranceM));
  const auto coordinates = Coordinates(g, snapped.get(), true);
  if (!coordinates) { return std::unexpected(coordinates.error()); }
  auto points = Subdivide(*coordinates, source.Coordinates, query.At.ToleranceM);
  if (!points) { return std::unexpected(points.error()); }
  if (!points->empty() && source.Segments.size() > kMaximumCandidates / points->size()) {
    return std::unexpected(Failure{HipRoofError::CapacityExceeded, "ownership candidate budget"});
  }
  Loop result;
  result.Points = std::move(*points);
  result.Owners.reserve(result.Points.size());
  for (size_t at = 0; at < result.Points.size(); ++at) {
    const auto owner =
        EdgeOwner(g, query, {result.Points[at], result.Points[(at + 1) % result.Points.size()]});
    if (!owner) { return std::unexpected(owner.error()); }
    result.Owners.push_back(*owner);
  }
  return result;
}

Checked<std::vector<Loop>> MaterialLoops(const Geos &g,
                                         const ContourGeometry &geometry,
                                         const ContourBoundary &source,
                                         const OwnershipView &query) {
  std::vector<Loop> result;
  const int count = GEOSGetNumGeometries_r(g.Handle, geometry.Material.get());
  if (count < 0) { return std::unexpected(Failure{HipRoofError::InvalidInput, "material shape"}); }
  for (int at = 0; at < count; ++at) {
    const auto *polygon = GEOSGetGeometryN_r(g.Handle, geometry.Material.get(), at);
    const int holes = GEOSGetNumInteriorRings_r(g.Handle, polygon);
    if (holes < 0) { return std::unexpected(Failure{"nonpolygon material"}); }
    for (int ring = 0; ring <= holes; ++ring) {
      const auto *boundary = ring == 0 ? GEOSGetExteriorRing_r(g.Handle, polygon)
                                       : GEOSGetInteriorRingN_r(g.Handle, polygon, ring - 1);
      auto loop = BoundLoop(g, boundary, geometry, source, query);
      if (!loop) { return std::unexpected(loop.error()); }
      result.push_back(std::move(*loop));
    }
  }
  return result;
}
}

Checked<std::vector<Loop>> Contours(std::span<const Loop> fronts,
                                    std::span<const Point> normals,
                                    std::span<const double> intercepts,
                                    Step step) {
  const Geos g;
  if (g.Handle == nullptr) { return std::unexpected(Failure{"GEOS context"}); }
  const auto source = CollectBoundary(
      fronts,
      {.Owners = std::min(normals.size(), intercepts.size()), .ToleranceM = step.ToleranceM});
  if (!source) { return std::unexpected(source.error()); }
  if (source->Segments.empty()) { return std::vector<Loop>{}; }
  const auto geometry =
      PrepareContourGeometry(g,
                             fronts,
                             source->Segments,
                             source->Coordinates,
                             {.ToleranceM = step.ToleranceM, .SignedAreaM2 = source->SignedAreaM2});
  if (!geometry) { return std::unexpected(geometry.error()); }
  const OwnershipView query{.Segments = source->Segments,
                            .Normals = normals,
                            .Intercepts = intercepts,
                            .Sources = geometry->Sources,
                            .At = step};
  auto result = MaterialLoops(g, *geometry, *source, query);
  if (!result) { return std::unexpected(result.error()); }
  if (result->empty()) { return result; }
  return Stitch(*result, step.ToleranceM);
}
}
