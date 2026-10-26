#include "PlanarRegionInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <set>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Spatial::Regions {
namespace {
Checked<void> ValidateBoundary(const State &state, std::span<const PlanarFaceRegion> regions) {
  if (auto valid = state.Check(); !valid) { return valid; }
  if (state.Points.size() > state.Budget.Points || state.Faces.size() > state.Budget.Faces ||
      regions.size() > state.Budget.Regions ||
      state.Points.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (!std::isfinite(state.Budget.CollinearityM) || state.Budget.CollinearityM < 0 ||
      state.Budget.CollinearityM > PlanarRegionBudget::kMaximumCollinearityM ||
      !std::isfinite(state.Budget.CoverageM2) || state.Budget.CoverageM2 < 0 ||
      state.Budget.CoverageM2 > PlanarRegionBudget::kMaximumCoverageM2) {
    return std::unexpected(Error::InvalidInput);
  }
  for (const auto point : state.Points) {
    if (!std::isfinite(point[0]) || !std::isfinite(point[1])) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  return {};
}

Checked<void> ValidateFace(const State &state, PlanarFace face) {
  for (const auto vertex : face) {
    if (vertex >= state.Points.size()) { return std::unexpected(Error::InvalidInput); }
  }
  const auto a = state.Points[face[0]];
  const auto b = state.Points[face[1]];
  const auto c = state.Points[face[2]];
  const double area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  if (!std::isfinite(area) || area <= 0) { return std::unexpected(Error::InvalidInput); }
  return {};
}

Checked<void> Validate(State &state, std::span<const PlanarFaceRegion> regions) {
  if (auto valid = ValidateBoundary(state, regions); !valid) { return valid; }
  std::vector<bool> selected(state.Faces.size());
  for (const auto &region : regions) {
    if (auto valid = state.Check(); !valid) { return valid; }
    if (region.empty() || region.size() > state.Faces.size()) {
      return std::unexpected(Error::InvalidInput);
    }
    for (const auto face : region) {
      if (face >= selected.size() || selected[face]) {
        return std::unexpected(Error::InvalidInput);
      }
      selected[face] = true;
      ++state.Output.Work.InputFaces;
    }
  }
  for (size_t at = 0; at < state.Faces.size(); ++at) {
    const auto face = state.Faces[at];
    if (auto valid = ValidateFace(state, face); !valid) { return valid; }
    if (!selected[at]) {
      ++state.UnselectedFaces;
      for (const auto vertex : face) { state.Protected.insert(state.Points[vertex]); }
    }
  }
  return {};
}

Checked<GeosGeometry>
Unite(const State &state, std::span<const uint32_t> region, PointIndex &lookup) {
  std::vector<GeosGeometry> parts;
  std::vector<GEOSGeometry *> pointers;
  std::set<uint32_t> ids;
  parts.reserve(region.size());
  pointers.reserve(region.size());
  for (const auto at : region) {
    if (auto valid = state.Check(); !valid) { return std::unexpected(valid.error()); }
    PolygonRing ring;
    for (const auto vertex : state.Faces[at]) {
      ring.push_back(state.Points[vertex]);
      if (ids.insert(vertex).second && !lookup.try_emplace(state.Points[vertex], vertex).second) {
        return std::unexpected(Error::AmbiguousVertices);
      }
    }
    auto polygon = MakeGeosPolygon(state.Context, std::span(&ring, 1));
    if (!polygon) { return std::unexpected(Error::LibraryFailure); }
    parts.push_back(std::move(polygon));
  }
  for (auto &part : parts) { pointers.push_back(part.release()); }
  const auto collection =
      state.Own(GEOSGeom_createCollection_r(state.Context,
                                            GEOS_GEOMETRYCOLLECTION,
                                            pointers.data(),
                                            static_cast<unsigned int>(pointers.size())));
  if (!collection) { return std::unexpected(Error::LibraryFailure); }
  auto shape = state.Own(GEOSUnaryUnion_r(state.Context, collection.get()));
  if (!shape) { return std::unexpected(Error::LibraryFailure); }
  return shape;
}

Checked<void>
PreserveProtected(const State &state, const PointIndex &lookup, std::span<const PlanarFace> faces) {
  std::set<uint32_t> retained;
  for (const auto face : faces) { retained.insert(face.begin(), face.end()); }
  for (const auto &[point, vertex] : lookup) {
    if (state.Protected.contains(point) && !retained.contains(vertex)) {
      return std::unexpected(Error::CoverageChanged);
    }
  }
  return {};
}
}
}

namespace outshine {
std::expected<PlanarRegionMesh, PlanarRegionError>
RemeshPlanarRegions(std::span<const PlanarPoint> points,
                    std::span<const PlanarFace> faces,
                    std::span<const PlanarFaceRegion> regions,
                    PlanarRegionBudget budget,
                    const std::stop_token &stop) {
  using namespace Spatial::Regions;
  State state{.Points = points,
              .Faces = faces,
              .Budget = budget,
              .Stop = stop,
              .Protected = {},
              .Output = {}};
  if (state.Context == nullptr) { return std::unexpected(Error::LibraryFailure); }
  if (auto valid = Validate(state, regions); !valid) { return std::unexpected(valid.error()); }
  state.Output.Regions.reserve(regions.size());
  for (const auto &region : regions) {
    PointIndex lookup;
    const auto shape = Unite(state, region, lookup);
    if (!shape) { return std::unexpected(shape.error()); }
    std::vector<PlanarFace> rebuilt;
    if (auto valid = TriangulateRegion(state, shape->get(), lookup, rebuilt); !valid) {
      return std::unexpected(valid.error());
    }
    if (auto valid = PreserveProtected(state, lookup, rebuilt); !valid) {
      return std::unexpected(valid.error());
    }
    state.Output.Regions.push_back(std::move(rebuilt));
  }
  if (auto valid = state.Check(); !valid) { return std::unexpected(valid.error()); }
  return std::move(state.Output);
}
}
