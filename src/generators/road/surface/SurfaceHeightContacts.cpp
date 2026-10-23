#include "SurfaceHeightProgramInternal.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Generators::HeightAssembly {
namespace {
template <class Emit>
bool Coordinates(GEOSContextHandle_t context, const GEOSGeometry *geometry, Emit emit) {
  const int kind = GEOSGeomTypeId_r(context, geometry);
  if (kind == GEOS_POINT) { return true; }
  if (kind == GEOS_LINESTRING || kind == GEOS_LINEARRING) {
    const auto *sequence = GEOSGeom_getCoordSeq_r(context, geometry);
    unsigned count = 0;
    if (sequence == nullptr || GEOSCoordSeq_getSize_r(context, sequence, &count) == 0) {
      return false;
    }
    for (unsigned at = 0; at < count; ++at) {
      PlanarPoint point{};
      if (GEOSCoordSeq_getXY_r(context, sequence, at, point.data(), &point[1]) == 0 ||
          !emit(point)) {
        return false;
      }
    }
    return true;
  }
  if (kind != GEOS_MULTILINESTRING && kind != GEOS_GEOMETRYCOLLECTION) { return false; }
  const int count = GEOSGetNumGeometries_r(context, geometry);
  if (count < 0) { return false; }
  for (int at = 0; at < count; ++at) {
    if (!Coordinates(context, GEOSGetGeometryN_r(context, geometry, at), emit)) { return false; }
  }
  return true;
}

bool ContactRow(const Input &input,
                const SurfaceHeightContact &contact,
                uint32_t at,
                PlanarPoint point,
                System &system) {
  if (!system.Count(system.Work.ContactSamples, system.Budget.ContactSamples)) { return false; }
  const auto face = input.Sources[contact.Source].Faces[at];
  const auto a = input.Points[face[0]];
  const auto u = Difference(input.Points[face[1]], a);
  const auto v = Difference(input.Points[face[2]], a);
  const auto delta = Difference(point, a);
  const double determinant = u[0] * v[1] - u[1] * v[0];
  const double s = (delta[0] * v[1] - delta[1] * v[0]) / determinant;
  const double t = (u[0] * delta[1] - u[1] * delta[0]) / determinant;
  Coefficients weights{1 - s - t, s, t};
  const double height =
      contact.Points.size() == 1 ? contact.Points[0][2] : HeightAt(contact.Points, point, system);
  constexpr double kContactRowScale = 1024;
  for (auto &value : weights) { value *= kContactRowScale; }
  system.Equal(face, weights, (height - system.Datum) * kContactRowScale);
  return system.Check();
}

bool ProfileSegments(const Input &input,
                     Workspace &workspace,
                     const SurfaceHeightContact &contact,
                     const GEOSGeometry *shape,
                     std::span<const PlanarPoint> xy,
                     System &system) {
  auto *const context = workspace.Geos.Handle;
  const auto &source = workspace.Sources[contact.Source];
  ++system.Work.GeometryQueries;
  return source.Index.Query(Bounds(xy), [&](uint32_t face) {
    if (!system.Count(system.Work.CandidateFaces, system.Budget.GeometryCandidates)) {
      return false;
    }
    const Geometry intersection{GEOSIntersection_r(context, source.Faces[face].get(), shape),
                                {context}};
    if (!intersection) { return system.Fail(Error::LibraryFailure); }
    if (!Coordinates(context, intersection.get(), [&](PlanarPoint point) {
          return ContactRow(input, contact, face, point, system);
        })) {
      return system.Fail(Error::LibraryFailure);
    }
    return true;
  });
}

bool ProfilePoint(const Input &input,
                  Workspace &workspace,
                  const SurfaceHeightContact &contact,
                  PlanarPoint point,
                  System &system) {
  auto *const context = workspace.Geos.Handle;
  auto &source = workspace.Sources[contact.Source];
  ++system.Work.GeometryQueries;
  Geometry shape{GEOSGeom_createPointFromXY_r(context, point[0], point[1]), {context}};
  if (!shape) { return system.Fail(Error::LibraryFailure); }
  size_t found = 0;
  const bool done = source.Index.Query(Bounds(std::span(&point, 1)), [&](uint32_t face) {
    if (!system.Count(system.Work.CandidateFaces, system.Budget.GeometryCandidates)) {
      return false;
    }
    const char covers = GEOSCovers_r(context, source.Faces[face].get(), shape.get());
    if (covers == 2) { return system.Fail(Error::LibraryFailure); }
    if (covers == 1) {
      ++found;
      return ContactRow(input, contact, face, point, system);
    }
    return true;
  });
  return done && (found != 0 || system.Fail(Error::InvalidInput));
}
}

bool Contacts(const Input &input, Workspace &workspace, System &system) {
  auto *const context = workspace.Geos.Handle;
  for (const auto &contact : input.Contacts) {
    if (!system.Check()) { return false; }
    std::vector<PlanarPoint> xy;
    xy.reserve(contact.Points.size());
    for (const auto point : contact.Points) { xy.push_back({point[0], point[1]}); }
    const auto &source = workspace.Sources[contact.Source];
    const Geometry shape =
        xy.size() == 1
            ? Geometry{GEOSGeom_createPointFromXY_r(context, xy[0][0], xy[0][1]), {context}}
            : Line(context, xy);
    if (!shape) { return system.Fail(Error::LibraryFailure); }
    const char covered = GEOSCovers_r(context, source.Domain.get(), shape.get());
    if (covered != 1) {
      return system.Fail(covered == 2 ? Error::LibraryFailure : Error::InvalidInput);
    }
    if (xy.size() > 1 && !ProfileSegments(input, workspace, contact, shape.get(), xy, system)) {
      return false;
    }
    for (const auto point : xy) {
      if (!ProfilePoint(input, workspace, contact, point, system)) { return false; }
    }
  }
  return true;
}
}
