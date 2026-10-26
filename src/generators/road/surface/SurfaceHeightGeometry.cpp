#include "SurfaceHeightProgramInternal.h"
#include "GeosPolygon.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::HeightAssembly {
Context::~Context() {
  if (Handle != nullptr) { GEOS_finish_r(Handle); }
}

Geometry Line(GEOSContextHandle_t context, std::span<const PlanarPoint> points) {
  auto *coordinates = GEOSCoordSeq_create_r(context, static_cast<unsigned>(points.size()), 2);
  if (coordinates == nullptr) { return {nullptr, {context}}; }
  for (unsigned at = 0; at < points.size(); ++at) {
    if (GEOSCoordSeq_setXY_r(context, coordinates, at, points[at][0], points[at][1]) == 0) {
      GEOSCoordSeq_destroy_r(context, coordinates);
      return {nullptr, {context}};
    }
  }
  return {GEOSGeom_createLineString_r(context, coordinates), {context}};
}

Box Bounds(std::span<const PlanarPoint> points) {
  Box bounds{.Min = {{points[0][0], points[0][1], 0}}, .Max = {{points[0][0], points[0][1], 0}}};
  for (const auto point : points) {
    for (size_t axis = 0; axis < 2; ++axis) {
      bounds.Min[axis] = std::min(bounds.Min[axis], point[axis]);
      bounds.Max[axis] = std::max(bounds.Max[axis], point[axis]);
    }
  }
  return bounds;
}

bool PrepareGeometry(const Input &input, Workspace &workspace, System &system) {
  auto *const context = workspace.Geos.Handle;
  if (context == nullptr) { return system.Fail(Error::LibraryFailure); }
  for (const auto &source : input.Sources) {
    if (!system.Check()) { return false; }
    SourceGeometry geometry{.Domain = Spatial::MakeGeosPolygon(context, source.Domain->Rings),
                            .Faces = {},
                            .Index = {}};
    if (!geometry.Domain) { return system.Fail(Error::LibraryFailure); }
    if (GEOSisValid_r(context, geometry.Domain.get()) != 1) {
      return system.Fail(Error::InvalidInput);
    }
    std::vector<Box> boxes;
    for (const auto face : source.Faces) {
      if (!system.Check()) { return false; }
      const PolygonRing ring{input.Points[face[0]], input.Points[face[1]], input.Points[face[2]]};
      auto shape = Spatial::MakeGeosPolygon(context, std::array{ring});
      if (!shape) { return system.Fail(Error::LibraryFailure); }
      boxes.push_back(Bounds(ring));
      geometry.Faces.push_back(std::move(shape));
    }
    auto index = PlanHierarchy::Build(boxes);
    if (!index) { return system.Fail(Error::LibraryFailure); }
    geometry.Index = std::move(*index);
    workspace.Sources.push_back(std::move(geometry));
  }
  return true;
}

bool Influence(const Input &input,
               Workspace &workspace,
               size_t owner,
               PlanarPoint a,
               PlanarPoint b,
               double threshold,
               std::vector<uint32_t> &faces,
               System &system) {
  if (!system.Check()) { return false; }
  auto *const context = workspace.Geos.Handle;
  const auto &source = input.Sources[owner];
  const std::array segment{a, b};
  const auto line = Line(context, segment);
  if (!line) { return system.Fail(Error::LibraryFailure); }
  constexpr int kBufferQuadrantSegments = 16;
  Geometry influence{GEOSBuffer_r(context, line.get(), source.Width, kBufferQuadrantSegments),
                     {context}};
  if (!influence) { return system.Fail(Error::LibraryFailure); }
  auto box = Bounds(segment);
  for (size_t axis = 0; axis < 2; ++axis) {
    box.Min[axis] -= source.Width;
    box.Max[axis] += source.Width;
  }
  ++system.Work.GeometryQueries;
  faces.clear();
  return workspace.Sources[owner].Index.Query(box, [&](uint32_t face) {
    if (!system.Count(system.Work.CandidateFaces, system.Budget.GeometryCandidates)) {
      return false;
    }
    const Geometry intersection{
        GEOSIntersection_r(context, workspace.Sources[owner].Faces[face].get(), influence.get()),
        {context}};
    double area = 0;
    if (!intersection || GEOSArea_r(context, intersection.get(), &area) == 0) {
      return system.Fail(Error::LibraryFailure);
    }
    if (area > threshold) { faces.push_back(face); }
    return true;
  });
}
}
