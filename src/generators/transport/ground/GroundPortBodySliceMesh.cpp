#include "GroundPortBodySliceInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators::GroundBodySlices {
namespace {
Error FromPlanar(PlanarMeshError error) {
  if (error == PlanarMeshError::Canceled) { return RoadGroundError::Canceled; }
  if (error == PlanarMeshError::CapacityExceeded) { return RoadGroundError::CapacityExceeded; }
  return HeightSolidError::InvalidInput;
}

Checked<PlanarStationMesh>
Clip(const Surface &surface, GroundPortBodyBudget budget, const std::stop_token &stop) {
  std::array<PlanarStationAxis, 4> axes;
  for (size_t axis = 0; axis < axes.size(); ++axis) {
    axes[axis].Values = surface.Values[axis];
    if (axis < 2 && surface.HasCut[axis]) { axes[axis].Cuts.push_back(surface.Cuts[axis]); }
  }
  auto mesh = ClipPlanarStations(surface.Points,
                                 surface.Faces,
                                 axes,
                                 surface.Above,
                                 {.Vertices = budget.Solid.Points,
                                  .Faces = std::min(budget.SurfaceFaces, budget.Solid.SurfaceFaces),
                                  .FaceVisits = budget.Solid.EdgeVisits},
                                 stop);
  if (!mesh) { return std::unexpected(FromPlanar(mesh.error())); }
  if (mesh->Mesh.Faces.empty()) {
    return std::unexpected(Error{HeightSolidError::Unrepresentable});
  }
  return std::move(*mesh);
}

Checked<HeightSolid> Solid(const PlanarStationMesh &mesh,
                           GroundPortBodyBudget budget,
                           GroundPortBodyWork &work,
                           size_t otherBytes,
                           const std::stop_token &stop) {
  constexpr auto absent = std::numeric_limits<uint32_t>::max();
  std::vector<uint32_t> remap(mesh.Mesh.Points.size(), absent);
  std::vector<PlanarPoint> points;
  std::vector<double> bottom;
  std::vector<PlanarFace> faces;
  std::vector<std::array<double, 3>> top;
  faces.reserve(mesh.Mesh.Faces.size());
  top.reserve(mesh.Mesh.Faces.size());
  for (const auto face : mesh.Mesh.Faces) {
    PlanarFace compact{};
    std::array<double, 3> heights{};
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto vertex = face[corner];
      if (remap[vertex] == absent) {
        remap[vertex] = static_cast<uint32_t>(points.size());
        points.push_back(mesh.Mesh.Points[vertex]);
        bottom.push_back(mesh.Stations[3][vertex]);
      }
      compact[corner] = remap[vertex];
      heights[corner] = mesh.Stations[2][vertex];
    }
    faces.push_back(compact);
    top.push_back(heights);
  }
  work.ScratchCapacityBytes = std::max(
      work.ScratchCapacityBytes,
      otherBytes + remap.capacity() * sizeof(uint32_t) + points.capacity() * sizeof(PlanarPoint) +
          bottom.capacity() * sizeof(double) + faces.capacity() * sizeof(PlanarFace) +
          top.capacity() * sizeof(std::array<double, 3>));
  work.CopiedSurfaceBytes += points.size() * (sizeof(PlanarPoint) + sizeof(double)) +
                             faces.size() * (sizeof(PlanarFace) + sizeof(std::array<double, 3>));
  auto body = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = top, .BottomHeights = bottom},
      budget.Solid,
      stop);
  if (!body) { return std::unexpected(Error{body.error()}); }
  return std::move(*body);
}

size_t HeapBytes(const PlanarStationMesh &mesh) {
  size_t bytes =
      mesh.Mesh.Points.capacity() * sizeof(PlanarPoint) +
      mesh.Mesh.Faces.capacity() * sizeof(PlanarFace) +
      (mesh.Mesh.PointRemap.capacity() + mesh.Mesh.FaceParents.capacity()) * sizeof(uint32_t) +
      mesh.SourceEdges.capacity() * sizeof(std::array<uint32_t, 2>) +
      mesh.Stations.capacity() * sizeof(std::vector<double>);
  for (const auto &values : mesh.Stations) { bytes += values.capacity() * sizeof(double); }
  return bytes;
}

bool Interface(const Surface &surface,
               const PlanarStationMesh &mesh,
               std::array<uint32_t, 2> edge) {
  for (size_t cut = 0; cut < 2; ++cut) {
    if (surface.HasCut[cut] && mesh.Stations[cut][edge[0]] == surface.Cuts[cut] &&
        mesh.Stations[cut][edge[1]] == surface.Cuts[cut]) {
      return true;
    }
  }
  const auto a = mesh.SourceEdges[edge[0]];
  const auto b = mesh.SourceEdges[edge[1]];
  const uint32_t low = std::min({a[0], a[1], b[0], b[1]});
  const uint32_t high = std::max({a[0], a[1], b[0], b[1]});
  if (high == std::numeric_limits<uint32_t>::max() || low == high) { return false; }
  for (const auto vertex : std::array{a[0], a[1], b[0], b[1]}) {
    if (vertex != low && vertex != high) { return false; }
  }
  return std::ranges::binary_search(surface.Interfaces, std::array{low, high});
}

bool OwnsContact(const Surface &surface, size_t face) {
  const auto vertices = surface.Faces[face];
  for (size_t cut = 0; cut < 2; ++cut) {
    if (!surface.HasCut[cut]) { continue; }
    const auto &values = surface.Values[cut];
    const double at = (values[vertices[0]] + values[vertices[1]] + values[vertices[2]]) / 3;
    const double boundary = surface.Cuts[cut];
    if (cut == 0) {
      if (surface.Above[cut] ? at < boundary : at > boundary) { return false; }
    } else if (surface.Above[cut] ? at <= boundary : at >= boundary) {
      return false;
    }
  }
  return true;
}

Checked<void> CopyContacts(GroundPortBody &body,
                           const Surface &surface,
                           const GroundPortBody &source,
                           GroundPortBodyWork &work,
                           const std::stop_token &stop) {
  if (source.TerrainContacts.size() != surface.Faces.size()) {
    return std::unexpected(Error{HeightSolidError::InvalidInput});
  }
  size_t count = 0;
  for (size_t face = 0; face < source.TerrainContacts.size(); ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    if (OwnsContact(surface, face)) { ++count; }
  }
  body.TerrainContacts.reserve(count);
  for (size_t face = 0; face < source.TerrainContacts.size(); ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    if (!OwnsContact(surface, face)) { continue; }
    body.TerrainContacts.push_back(source.TerrainContacts[face]);
    ++work.TerrainContactFaces;
    work.CopiedSurfaceBytes += sizeof(EarthworkStamp) + source.TerrainContacts[face].HeapBytes();
  }
  work.FaceVisits += 2 * source.TerrainContacts.size();
  return {};
}

Checked<void> Finish(GroundPortBody &body,
                     const Surface &surface,
                     const PlanarStationMesh &mesh,
                     const GroundPortBody &source,
                     GroundPortBodyWork &work,
                     const std::stop_token &stop) {
  auto &geometry = body.Geometry;
  for (size_t wall = 0; wall < geometry.WallKinds.size(); ++wall) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    const auto edge = HeightSolidWallSurfaceEdge(geometry, wall);
    if (!edge) { return std::unexpected(Error{HeightSolidError::InvalidInput}); }
    const auto parent = geometry.WallParents[wall];
    const auto top = geometry.Faces[parent];
    std::array<uint32_t, 2> original{};
    for (size_t end = 0; end < 2; ++end) {
      for (size_t corner = 0; corner < 3; ++corner) {
        if (top[corner] == (*edge)[end]) { original[end] = mesh.Mesh.Faces[parent][corner]; }
      }
    }
    if (Interface(surface, mesh, original)) {
      geometry.WallKinds[wall] = HeightSolidWallKind::Interface;
    }
  }
  return CopyContacts(body, surface, source, work, stop);
}
}

Checked<GroundPortBody> Build(const GroundPortField &field,
                              const GroundPortBody &source,
                              const TransportCoreSegment &interval,
                              GroundPortBodyBudget budget,
                              GroundPortBodyWork &work,
                              const std::stop_token &stop) {
  auto surface = Prepare(field, source, interval, budget, work, stop);
  if (!surface) { return std::unexpected(surface.error()); }
  auto mesh = Clip(*surface, budget, stop);
  if (!mesh) { return std::unexpected(mesh.error()); }
  auto solid = Solid(*mesh, budget, work, surface->HeapBytes() + HeapBytes(*mesh), stop);
  if (!solid) { return std::unexpected(solid.error()); }
  GroundPortBody body{.Source = source.Source,
                      .SourceBegin = interval.Ends[0].Fraction,
                      .SourceEnd = interval.Ends[1].Fraction,
                      .Geometry = std::move(*solid),
                      .FaceParents = {},
                      .TerrainContacts = {}};
  body.FaceParents.reserve(mesh->Mesh.FaceParents.size());
  for (const auto parent : mesh->Mesh.FaceParents) {
    body.FaceParents.push_back(source.FaceParents[parent]);
  }
  if (auto finished = Finish(body, *surface, *mesh, source, work, stop); !finished) {
    return std::unexpected(finished.error());
  }
  work.FaceVisits += mesh->Work.FaceVisits;
  work.RemappedVertices += mesh->Mesh.Points.size();
  work.SolidEdgeVisits += body.Geometry.Work.EdgeVisits;
  return body;
}
}
