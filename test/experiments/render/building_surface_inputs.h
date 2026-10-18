#ifndef OUTSHINE_TEST_BUILDING_SURFACE_INPUTS_H
#define OUTSHINE_TEST_BUILDING_SURFACE_INPUTS_H

#include "BuildingPreparation.h"
#include "BuildingSurface.h"
#include "RoofSurface.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
#include <utility>

namespace outshine::Test {
using SurfaceJson = nlohmann::json;

inline SurfaceJson InputVector(const Vec3 &value) {
  return {value[0], value[1], value[2]};
}

inline SurfaceJson InputPoint(EastNorth value) {
  return {value.EastM, value.NorthM};
}

inline SurfaceJson InputRing(std::span<const EastNorth> ring) {
  auto result = SurfaceJson::array();
  for (const auto point : ring) { result.push_back(InputPoint(point)); }
  return result;
}

inline SurfaceJson InputHip(const Generators::BuildingShape &shape) {
  if (!shape.HipRoof) { return {{"available", false}, {"reason", "no cached native hip plan"}}; }
  if (!*shape.HipRoof) {
    return {{"available", false}, {"native_error", static_cast<unsigned>(shape.HipRoof->error())}};
  }
  const auto &plan = **shape.HipRoof;
  auto facets = SurfaceJson::array();
  for (const auto &facet : plan.Facets()) {
    facets.push_back({{"ring", InputRing(facet.Ring)},
                      {"gradient", InputPoint(facet.Gradient)},
                      {"intercept_m", facet.InterceptM},
                      {"source_ring", facet.SourceRing},
                      {"source_edge", facet.SourceEdge}});
  }
  return {{"available", true},
          {"origin", InputPoint(plan.Origin())},
          {"maximum_distance_m", plan.MaximumDistanceM()},
          {"precision_m", plan.PrecisionM()},
          {"facets", std::move(facets)}};
}

inline bool HasCachedRoof(const Generators::BuildingShape &shape) {
  return shape.Roof != Generators::RoofKind::Hip || (shape.HipRoof && shape.HipRoof->has_value());
}

inline SurfaceJson InputShape(const Generators::BuildingShape &shape, double minimumHeightM) {
  using namespace Generators;
  auto holes = SurfaceJson::array();
  for (const auto &hole : shape.Holes) { holes.push_back(InputRing(hole)); }
  const double deckM =
      shape.SeatM + shape.FootM + shape.EavesM + (shape.Roof == RoofKind::Flat ? shape.RiseM : 0.0);
  SurfaceJson result{{"ring", InputRing(shape.Ring)},
                     {"holes", std::move(holes)},
                     {"bottom_m", BuildingBottomM(shape, minimumHeightM)},
                     {"roof_deck_m", deckM},
                     {"roof_kind", static_cast<unsigned>(shape.Roof)},
                     {"rise_m", shape.RiseM},
                     {"centre", InputPoint(shape.Centre)},
                     {"axis_u", InputPoint(shape.AxisU)},
                     {"half_u_m", shape.HalfUm},
                     {"half_v_m", shape.HalfVm},
                     {"break_fraction_v", shape.BreakFracV},
                     {"break_rise_m", shape.BreakRiseM},
                     {"period_m", shape.PeriodM},
                     {"party_wall_edges", shape.PartyWallEdges}};
  if (shape.Roof == RoofKind::Hip) { result["hip"] = InputHip(shape); }
  result["height_controls"] = SurfaceJson::array();
  if (HasCachedRoof(shape)) {
    const RoofSurface roof(shape);
    const auto sample = [&](EastNorth point) {
      result["height_controls"].push_back(
          {{"point", InputPoint(point)},
           {"inside", roof.Contains(point)},
           {"height_m", roof.HeightAt(point)},
           {"wall_top_m", deckM + std::max(roof.HeightAt(point), 0.0)}});
    };
    sample(shape.Centre);
    for (const auto point : shape.Ring) { sample(point); }
    for (const auto &hole : shape.Holes) {
      for (const auto point : hole) { sample(point); }
    }
  }
  return result;
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
    const auto face = surface.FaceAt(index);
    const std::array faces{face};
    result["faces"].push_back({{"index", index},
                               {"part", face.Part},
                               {"side", face.Side},
                               {"estimated_triangles", surface.EstimatedTriangles(faces)}});
  }
  result["rays"] = InputRays(surface, eye);
  return result;
}
}

#endif
