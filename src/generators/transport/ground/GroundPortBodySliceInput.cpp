#include "GroundPortBodySliceInternal.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Generators::GroundBodySlices {
namespace {
struct Plane {
  EastNorthUp Normal{};
  double Offset = 0;

  [[nodiscard]] double At(const std::array<double, 3> &point) const noexcept {
    return Normal.EastM * point[0] + Normal.NorthM * point[1] + Normal.UpM * point[2];
  }
};

Checked<std::array<Plane, 2>> Planes(const GroundPortField &field,
                                     const GroundPortBody &body,
                                     const TransportCoreSegment &interval,
                                     Surface &surface,
                                     const std::stop_token &stop) {
  std::array<GroundPortFieldRequestPoint, 2> requests;
  std::array<size_t, 2> ends{};
  size_t count = 0;
  surface.HasCut = {interval.Ends[0].Fraction > 0, interval.Ends[1].Fraction < 1};
  for (size_t at = 0; at < 2; ++at) {
    if (!surface.HasCut[at]) { continue; }
    ends[count] = at;
    requests[count++] = {.Edge = body.Source, .Anchor = interval.Ends[at].Geographic};
  }
  auto samples = field.Sample(std::span(requests).first(count), {}, stop);
  if (!samples) {
    return std::unexpected(std::visit([](auto error) -> Error { return error; }, samples.error()));
  }
  const auto source = TangentFrame::At(field.Domain().Anchor);
  std::array<Plane, 2> result{};
  for (size_t at = 0; at < count; ++at) {
    const auto end = ends[at];
    const auto &port = samples->Ports[at].Port;
    const auto target = TangentFrame::At(interval.Ends[end].Geographic);
    const auto normal = source.ToLocalDirection(
        target.ToEcefDirection({.EastM = port.Gate.OutE, .NorthM = port.Gate.OutN, .UpM = 0}));
    const auto centre =
        source.ToLocalPosition({.LongitudeDeg = interval.Ends[end].Geographic.LongitudeDeg,
                                .LatitudeDeg = interval.Ends[end].Geographic.LatitudeDeg,
                                .HeightM = port.Gate.GradeM});
    result[end].Normal = normal;
    result[end].Offset = result[end].At({centre.EastM, centre.NorthM, centre.UpM});
    surface.Cuts[end] = result[end].Offset;
  }
  const bool forward = body.Source.Direction == World::EdgeDirection::Forward;
  surface.Above = {forward, !forward, true, true};
  return result;
}

Checked<void>
CopyInterfaces(const HeightSolid &geometry, std::span<const uint32_t> remap, Surface &surface) {
  constexpr auto absent = std::numeric_limits<uint32_t>::max();
  for (size_t wall = 0; wall < geometry.WallKinds.size(); ++wall) {
    if (geometry.WallKinds[wall] != HeightSolidWallKind::Interface) { continue; }
    const auto edge = HeightSolidWallSurfaceEdge(geometry, wall);
    if (!edge || remap[(*edge)[0]] == absent || remap[(*edge)[1]] == absent) {
      return std::unexpected(Error{HeightSolidError::InvalidInput});
    }
    std::array compact{remap[(*edge)[0]], remap[(*edge)[1]]};
    std::ranges::sort(compact);
    surface.Interfaces.push_back(compact);
  }
  std::ranges::sort(surface.Interfaces);
  const auto duplicates = std::ranges::unique(surface.Interfaces);
  surface.Interfaces.erase(duplicates.begin(), duplicates.end());
  return {};
}

Checked<void> Copy(const GroundPortField &field,
                   const GroundPortBody &body,
                   const std::array<Plane, 2> &planes,
                   Surface &surface,
                   const std::stop_token &stop) {
  constexpr auto absent = std::numeric_limits<uint32_t>::max();
  const auto &geometry = body.Geometry;
  const auto &mesh = field.Geometry();
  std::vector<uint32_t> remap(geometry.Points.size(), absent);
  const size_t pointsMost = std::min(geometry.Points.size(), geometry.SurfaceFaces * 3);
  surface.Points.reserve(pointsMost);
  for (auto &values : surface.Values) { values.reserve(pointsMost); }
  surface.Faces.reserve(geometry.SurfaceFaces);
  for (size_t at = 0; at < geometry.SurfaceFaces; ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    const auto parent = body.FaceParents[at];
    if (parent >= mesh.Surface.Faces.size()) {
      return std::unexpected(Error{HeightSolidError::InvalidInput});
    }
    const auto bound = field.SourceEdgeOfFace(parent);
    if (!bound || *bound != body.Source) {
      return std::unexpected(Error{HeightSolidError::InvalidInput});
    }
    PlanarFace face{};
    const auto top = geometry.Faces[at];
    const auto bottom = geometry.Faces[geometry.SurfaceFaces + at];
    for (size_t corner = 0; corner < 3; ++corner) {
      if (top[corner] >= geometry.Points.size() || bottom[2 - corner] >= geometry.Points.size()) {
        return std::unexpected(Error{HeightSolidError::InvalidInput});
      }
      const auto point = geometry.Points[top[corner]];
      const auto under = geometry.Points[bottom[2 - corner]];
      const auto original = mesh.Surface.Faces[parent][corner];
      if (original >= mesh.Surface.Points.size() || original >= mesh.HeightsM.size() ||
          point[0] != mesh.Surface.Points[original][0] ||
          point[1] != mesh.Surface.Points[original][1] || point[2] != mesh.HeightsM[original] ||
          under[0] != point[0] || under[1] != point[1] || !std::isfinite(under[2]) ||
          under[2] >= point[2]) {
        return std::unexpected(Error{HeightSolidError::InvalidInput});
      }
      if (remap[top[corner]] == absent) {
        remap[top[corner]] = static_cast<uint32_t>(surface.Points.size());
        surface.Points.push_back({point[0], point[1]});
        surface.Values[0].push_back(planes[0].At(point));
        surface.Values[1].push_back(planes[1].At(point));
        surface.Values[2].push_back(point[2]);
        surface.Values[3].push_back(under[2]);
      }
      face[corner] = remap[top[corner]];
    }
    surface.Faces.push_back(face);
  }
  return CopyInterfaces(geometry, remap, surface);
}
}

Checked<Surface> Prepare(const GroundPortField &field,
                         const GroundPortBody &body,
                         const TransportCoreSegment &interval,
                         GroundPortBodyBudget budget,
                         GroundPortBodyWork &work,
                         const std::stop_token &stop) {
  const auto &geometry = body.Geometry;
  if (geometry.SurfaceFaces == 0 || geometry.SurfaceFaces != body.FaceParents.size() ||
      geometry.SurfaceFaces != body.TerrainContacts.size() ||
      geometry.SurfaceFaces > geometry.Faces.size() / 2) {
    return std::unexpected(Error{HeightSolidError::InvalidInput});
  }
  if (geometry.SurfaceFaces > budget.SurfaceFaces ||
      geometry.SurfaceFaces > budget.Solid.SurfaceFaces ||
      geometry.Points.size() > budget.Solid.Vertices) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  Surface surface;
  const auto planes = Planes(field, body, interval, surface, stop);
  if (!planes) { return std::unexpected(planes.error()); }
  if (auto copied = Copy(field, body, *planes, surface, stop); !copied) {
    return std::unexpected(copied.error());
  }
  work.ScratchCapacityBytes = std::max(
      work.ScratchCapacityBytes, surface.HeapBytes() + geometry.Points.size() * sizeof(uint32_t));
  work.FaceVisits += geometry.SurfaceFaces;
  work.CopiedSurfaceBytes += surface.Points.size() * (sizeof(PlanarPoint) + 4 * sizeof(double)) +
                             surface.Faces.size() * sizeof(PlanarFace);
  return surface;
}
}
