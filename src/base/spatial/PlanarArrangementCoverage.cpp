#include "PlanarArrangementInternal.h"
#include "GeosPolygon.h"
#include <geos_c.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine::PlanarArrangements {
Checked<uint64_t> Owners(const Context &context,
                         const GEOSGeometry *shape,
                         std::span<const Prepared> domains,
                         PlanarArrangementWork &work,
                         size_t maximumTests) {
  if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto point = context.Own(GEOSPointOnSurface_r(context.Handle, shape));
  if (!point) { return std::unexpected(context.Failure()); }
  uint64_t owners = 0;
  for (size_t at = 0; at < domains.size(); ++at) {
    if (work.DomainTests == maximumTests) { return std::unexpected(Error::CapacityExceeded); }
    ++work.DomainTests;
    const char covered = GEOSPreparedCovers_r(context.Handle, domains[at].get(), point.get());
    if (covered == 2) { return std::unexpected(context.Failure()); }
    if (covered == 1) { owners |= uint64_t{1} << at; }
  }
  return owners;
}

namespace {
Checked<Geometry> FaceGeometry(const Context &context, const PlanarMesh &mesh, PlanarFace face) {
  const PolygonRing ring{mesh.Points[face[0]], mesh.Points[face[1]], mesh.Points[face[2]]};
  auto polygon = Spatial::MakeGeosPolygon(context.Handle, std::span(&ring, 1));
  if (!polygon) { return std::unexpected(context.Failure()); }
  return polygon;
}

Checked<void> CheckDomain(const Context &context,
                          const GEOSGeometry *domain,
                          std::span<const Geometry> triangles,
                          std::span<const uint64_t> owners,
                          size_t index,
                          PlanarArrangementWork &work,
                          double maximumDifference) {
  std::vector<Geometry> selected;
  for (size_t at = 0; at < triangles.size(); ++at) {
    if ((owners[at] & (uint64_t{1} << index)) == 0) { continue; }
    auto copy = context.Own(GEOSGeom_clone_r(context.Handle, triangles[at].get()));
    if (!copy) { return std::unexpected(context.Failure()); }
    selected.push_back(std::move(copy));
  }
  const auto collection = Collection(context, std::move(selected));
  if (!collection) { return std::unexpected(context.Failure()); }
  const auto actual = context.Own(GEOSUnaryUnion_r(context.Handle, collection.get()));
  if (!actual) { return std::unexpected(context.Failure()); }
  const auto difference = context.Own(GEOSSymDifference_r(context.Handle, domain, actual.get()));
  if (!difference) { return std::unexpected(context.Failure()); }
  double area = 0;
  if (GEOSArea_r(context.Handle, difference.get(), &area) == 0 || !std::isfinite(area)) {
    return std::unexpected(context.Failure());
  }
  work.MaximumAreaDifferenceM2 = std::max(work.MaximumAreaDifferenceM2, area);
  if (area > maximumDifference) { return std::unexpected(Error::FootprintChanged); }
  return {};
}
}

Checked<void> AssignAndValidate(const Context &context,
                                PlanarArrangement &arrangement,
                                std::span<const Geometry> domains,
                                std::span<const Prepared> prepared,
                                PlanarArrangementRecipe recipe,
                                PlanarArrangementBudget budget) {
  std::vector<Geometry> triangles;
  triangles.reserve(arrangement.Mesh.Faces.size());
  arrangement.FaceDomains.reserve(arrangement.Mesh.Faces.size());
  for (const auto face : arrangement.Mesh.Faces) {
    auto shape = FaceGeometry(context, arrangement.Mesh, face);
    if (!shape) { return std::unexpected(shape.error()); }
    const auto owners =
        Owners(context, shape->get(), prepared, arrangement.Work, budget.DomainTests);
    if (!owners) { return std::unexpected(owners.error()); }
    if (*owners == 0) { return std::unexpected(Error::FootprintChanged); }
    arrangement.FaceDomains.push_back(*owners);
    triangles.push_back(std::move(*shape));
  }
  for (size_t at = 0; at < domains.size(); ++at) {
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (auto result = CheckDomain(context,
                                  domains[at].get(),
                                  triangles,
                                  arrangement.FaceDomains,
                                  at,
                                  arrangement.Work,
                                  recipe.MaximumAreaDifferenceM2);
        !result) {
      return result;
    }
  }
  const auto collection = Collection(context, std::move(triangles));
  if (!collection) { return std::unexpected(context.Failure()); }
  const int valid = GEOSCoverageIsValid_r(context.Handle, collection.get(), 0, nullptr);
  if (valid == 2) { return std::unexpected(context.Failure()); }
  if (valid != 1) { return std::unexpected(Error::FootprintChanged); }
  return {};
}
}
