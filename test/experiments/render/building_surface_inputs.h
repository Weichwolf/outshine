#ifndef OUTSHINE_TEST_BUILDING_SURFACE_INPUTS_H
#define OUTSHINE_TEST_BUILDING_SURFACE_INPUTS_H

#include "building_roof_inputs.h"
#include "BuildingSurface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>
#include <utility>

namespace outshine::Test {
inline SurfaceJson InputWallNormal(const Generators::BuildingShape &shape, size_t side) {
  size_t edge = side - 2;
  std::span<const EastNorth> ring = shape.Ring;
  bool hole = false;
  if (edge >= ring.size()) {
    edge -= ring.size();
    for (const auto &inner : shape.Holes) {
      if (edge < inner.size()) {
        ring = inner;
        hole = true;
        break;
      }
      edge -= inner.size();
    }
  }
  if (edge >= ring.size()) { return nullptr; }
  double area = 0.0;
  for (size_t at = 0; at < ring.size(); ++at) {
    const auto a = ring[at];
    const auto b = ring[(at + 1) % ring.size()];
    area += a.EastM * b.NorthM - a.NorthM * b.EastM;
  }
  const auto a = ring[edge];
  const auto b = ring[(edge + 1) % ring.size()];
  const double length = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
  if (length == 0.0) { return nullptr; }
  const double sign = (area >= 0.0) != hole ? 1.0 : -1.0;
  return {sign * (b.NorthM - a.NorthM) / length, -sign * (b.EastM - a.EastM) / length, 0.0};
}

inline SurfaceJson InputFace(const Generators::BuildingSurface &surface, size_t index) {
  const auto face = surface.FaceAt(index);
  const auto &shape = surface.Shapes()[face.Part];
  const std::array faces{face};
  SurfaceJson normal = nullptr;
  if (face.Side == 0) { normal = SurfaceJson{0, 0, -1}; }
  if (face.Side >= 2) { normal = InputWallNormal(shape, face.Side); }
  return {{"index", index},
          {"part", face.Part},
          {"side", face.Side},
          {"estimated_triangles", surface.EstimatedTriangles(faces)},
          {"normal_enu", std::move(normal)}};
}

inline SurfaceJson InputRay(const Generators::BuildingSurface &surface,
                            const Vec3 &origin,
                            const Vec3 &direction,
                            double maximum) {
  std::vector<double> cuts;
  const auto hit = surface.Trace({.Origin = origin, .Direction = direction}, 0.0, maximum, cuts);
  SurfaceJson result{{"origin", InputVector(origin)},
                     {"direction", InputVector(direction)},
                     {"minimum", 0.0},
                     {"maximum", maximum},
                     {"hit", nullptr}};
  if (hit) {
    result["hit"] = {{"along", hit->Along},
                     {"normal", InputVector(hit->Normal)},
                     {"part", hit->Part},
                     {"side", hit->Face},
                     {"face", surface.FaceIndex(*hit)},
                     {"point", InputVector(origin + direction * hit->Along)}};
  }
  return result;
}

inline SurfaceJson InputRays(const Generators::BuildingSurface &surface,
                             const std::optional<Vec3> &eye) {
  const auto shapes = surface.Shapes();
  if (shapes.empty() || surface.Bounds().Empty() || !std::ranges::all_of(shapes, HasCachedRoof)) {
    return nullptr;
  }
  const auto centre = surface.Bounds().Middle();
  const auto span = surface.Bounds().Span();
  const double reach = std::hypot(span[0], span[1], span[2]) + 1.0;
  auto rays = SurfaceJson::array();
  for (size_t axis = 0; axis < 3; ++axis) {
    for (const double sign : {-1.0, 1.0}) {
      Vec3 direction{};
      direction[axis] = sign;
      rays.push_back(InputRay(surface, centre - direction * reach, direction, 2 * reach));
    }
  }
  if (eye) {
    const auto offset = centre - *eye;
    const double distance = std::hypot(offset[0], offset[1], offset[2]);
    if (distance > 0.0) {
      rays.push_back(InputRay(surface, *eye, offset * (1.0 / distance), distance + reach));
    }
  }
  return rays;
}

inline SurfaceJson InputSurface(const Generators::BuildingSurface &surface,
                                double minimumHeightM,
                                const std::optional<Vec3> &eye) {
  const auto &axes = surface.Axes();
  SurfaceJson result{{"origin", InputVector(surface.Origin())},
                     {"east", InputVector(axes.East)},
                     {"north", InputVector(axes.North)},
                     {"up", InputVector(axes.Up)},
                     {"minimum_height_m", minimumHeightM},
                     {"projectable", surface.SupportsProjection()},
                     {"opaque_for_coverage", nullptr},
                     {"interior_topology", nullptr},
                     {"fine_openings_represented", false},
                     {"shapes", SurfaceJson::array()},
                     {"faces", SurfaceJson::array()}};
  for (const auto &shape : surface.Shapes()) {
    result["shapes"].push_back(InputShape(shape, minimumHeightM));
  }
  for (size_t index = 0; index < surface.FaceCount(); ++index) {
    result["faces"].push_back(InputFace(surface, index));
  }
  result["rays"] = InputRays(surface, eye);
  return result;
}
}

#endif
