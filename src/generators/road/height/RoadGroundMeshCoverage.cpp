#include "RoadGroundMeshInternal.h"
#include "GeosTriangle.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>
#include <geos_c.h>

namespace outshine::Generators::RoadGround {
namespace {
constexpr double kAreaErrorMostM2 = 1e-6;

struct Edge {
  std::array<uint32_t, 2> Key{};
  uint32_t Face = 0;
};

std::vector<Edge> Edges(std::span<const PlanarFace> faces) {
  std::vector<Edge> edges;
  edges.reserve(3 * faces.size());
  for (size_t at = 0; at < faces.size(); ++at) {
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto a = faces[at][corner];
      const auto b = faces[at][(corner + 1) % 3];
      edges.push_back({.Key = {std::min(a, b), std::max(a, b)}, .Face = static_cast<uint32_t>(at)});
    }
  }
  std::ranges::sort(edges, {}, &Edge::Key);
  return edges;
}

MeshResult<void> Anchors(const RoadGroundField &source, const PlanarMesh &mesh) {
  if (mesh.PointRemap.size() != source.Priors.size() || mesh.Points.size() < source.Priors.size()) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  std::vector<uint8_t> used(source.Priors.size());
  for (size_t at = 0; at < used.size(); ++at) {
    const auto &p = source.Priors[at].At;
    if (mesh.PointRemap[at] != at || mesh.Points[at] != PlanarPoint{p.EastM, p.NorthM}) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
  }
  for (const auto face : mesh.Faces) {
    for (const auto vertex : face) {
      if (vertex < used.size()) { used[vertex] = 1; }
    }
  }
  if (std::ranges::find(used, uint8_t{0}) != used.end()) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  return {};
}

MeshResult<void> WholePorts(const RoadGroundField &source, const PlanarMesh &mesh) {
  const auto original = Edges(source.Triangles);
  const auto current = Edges(mesh.Faces);
  for (size_t first = 0; first < original.size();) {
    size_t last = first + 1;
    while (last < original.size() && original[last].Key == original[first].Key) { ++last; }
    if (last - first > 2) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    if (last - first == 2 &&
        source.Faces[original[first].Face].Patch != source.Faces[original[first + 1].Face].Patch) {
      const auto range = std::ranges::equal_range(current, original[first].Key, {}, &Edge::Key);
      if (range.size() != 2) { return std::unexpected(RoadGroundError::InvalidGeometry); }
      const auto a = mesh.FaceParents[range[0].Face];
      const auto b = mesh.FaceParents[range[1].Face];
      if (std::minmax(a, b) != std::minmax(original[first].Face, original[first + 1].Face)) {
        return std::unexpected(RoadGroundError::InvalidGeometry);
      }
    }
    first = last;
  }
  return {};
}

Spatial::GeosGeometry Collection(GEOSContextHandle_t context,
                                 std::span<const PlanarPoint> points,
                                 std::span<const PlanarFace> faces) {
  std::vector<Spatial::GeosGeometry> owners;
  owners.reserve(faces.size());
  std::vector<GEOSGeometry *> raw;
  raw.reserve(faces.size());
  for (const auto face : faces) {
    std::array<Vec3, 3> corners{};
    for (size_t at = 0; at < 3; ++at) {
      corners[at] = {{points[face[at]][0], points[face[at]][1], 0}};
    }
    owners.push_back(Spatial::ProjectedTriangle(context, corners));
    if (!owners.back()) { return {nullptr, {context}}; }
    raw.push_back(owners.back().get());
  }
  Spatial::GeosGeometry result{
      GEOSGeom_createCollection_r(
          context, GEOS_GEOMETRYCOLLECTION, raw.data(), static_cast<unsigned>(raw.size())),
      {context}};
  if (result) {
    for (size_t at = 0; at < owners.size(); ++at) { raw[at] = owners[at].release(); }
  }
  return result;
}

bool ValidCoverage(GEOSContextHandle_t context, const GEOSGeometry *collection) {
  GEOSGeometry *invalid = nullptr;
  const int valid = GEOSCoverageIsValid_r(context, collection, 0, &invalid);
  const Spatial::GeosGeometry owned{invalid, {context}};
  return valid == 1;
}

MeshResult<void>
GeometryCoverage(const RoadGroundField &source, const PlanarMesh &mesh, RoadGroundMeshWork &work) {
  const std::unique_ptr<GEOSContextHandle_HS, decltype(&GEOS_finish_r)> context(GEOS_init_r(),
                                                                                &GEOS_finish_r);
  if (!context) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  std::vector<PlanarPoint> points;
  points.reserve(source.Priors.size());
  for (const auto &prior : source.Priors) { points.push_back({prior.At.EastM, prior.At.NorthM}); }
  const auto original = Collection(context.get(), points, source.Triangles);
  const auto current = Collection(context.get(), mesh.Points, mesh.Faces);
  if (!original || !current || !ValidCoverage(context.get(), original.get()) ||
      !ValidCoverage(context.get(), current.get())) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  const Spatial::GeosGeometry originalUnion{GEOSCoverageUnion_r(context.get(), original.get()),
                                            {context.get()}};
  const Spatial::GeosGeometry currentUnion{GEOSCoverageUnion_r(context.get(), current.get()),
                                           {context.get()}};
  if (!originalUnion || !currentUnion) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  const Spatial::GeosGeometry difference{
      GEOSSymDifference_r(context.get(), originalUnion.get(), currentUnion.get()), {context.get()}};
  if (!difference || GEOSArea_r(context.get(), difference.get(), &work.CoverageErrorM2) == 0 ||
      !std::isfinite(work.CoverageErrorM2) || work.CoverageErrorM2 > kAreaErrorMostM2) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  return {};
}
}

MeshResult<void> CheckMeshCoverage(const RoadGroundField &source,
                                   const PlanarMesh &mesh,
                                   RoadGroundMeshWork &work,
                                   const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
  if (mesh.Faces.size() != mesh.FaceParents.size()) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  std::vector<double> areas(source.Faces.size());
  for (size_t at = 0; at < mesh.Faces.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    const auto parent = mesh.FaceParents[at];
    if (parent >= areas.size()) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    const auto &face = mesh.Faces[at];
    for (const auto vertex : face) {
      if (vertex >= mesh.Points.size()) {
        return std::unexpected(RoadGroundError::InvalidGeometry);
      }
    }
    const double area =
        MeshArea(mesh.Points[face[0]], mesh.Points[face[1]], mesh.Points[face[2]]) / 2;
    if (!(area > 0) || !std::isfinite(area)) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
    areas[parent] += area;
  }
  for (size_t at = 0; at < areas.size(); ++at) {
    const auto &t = source.Faces[at].Triangle;
    const double expected =
        MeshArea({t[0].EastM, t[0].NorthM}, {t[1].EastM, t[1].NorthM}, {t[2].EastM, t[2].NorthM}) /
        2;
    work.MaximumRootAreaErrorM2 =
        std::max(work.MaximumRootAreaErrorM2, std::abs(areas[at] - expected));
    if (!(expected > 0) || !std::isfinite(expected) ||
        std::abs(areas[at] - expected) > kAreaErrorMostM2) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
  }
  if (const auto anchors = Anchors(source, mesh); !anchors) { return anchors; }
  if (const auto ports = WholePorts(source, mesh); !ports) { return ports; }
  if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
  return GeometryCoverage(source, mesh, work);
}
}
