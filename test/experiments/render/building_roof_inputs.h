#ifndef OUTSHINE_TEST_BUILDING_ROOF_INPUTS_H
#define OUTSHINE_TEST_BUILDING_ROOF_INPUTS_H

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

inline SurfaceJson InputRoofBand(const Generators::BuildingShape &shape,
                                 double heightAtZeroM,
                                 double slope,
                                 SurfaceJson interval) {
  const auto v = shape.AxisV();
  const Vec3 normal{{-slope * v.EastM, -slope * v.NorthM, 1}};
  return {
      {"v_interval", std::move(interval)},
      {"height_at_v0_m", heightAtZeroM},
      {"slope_v", slope},
      {"normal_enu", InputVector(normal * (1.0 / std::hypot(normal[0], normal[1], normal[2])))}};
}

inline SurfaceJson InputRoofBands(const Generators::BuildingShape &shape, double deckM) {
  using namespace Generators;
  auto bands = SurfaceJson::array();
  const auto add = [&](double height,
                       double slope,
                       SurfaceJson low,
                       SurfaceJson high,
                       bool lowClosed,
                       bool highClosed) {
    bands.push_back(InputRoofBand(shape,
                                  height,
                                  slope,
                                  {{"min", std::move(low)},
                                   {"max", std::move(high)},
                                   {"min_closed", lowClosed},
                                   {"max_closed", highClosed}}));
  };
  if (shape.Roof == RoofKind::Flat || shape.RiseM == 0.0) {
    add(deckM, 0.0, nullptr, nullptr, false, false);
  } else if (shape.Roof == RoofKind::Shed) {
    add(deckM + shape.RiseM * 0.5,
        shape.RiseM / (2 * shape.HalfVm),
        nullptr,
        nullptr,
        false,
        false);
  } else if (shape.Roof == RoofKind::Gable) {
    add(deckM + shape.RiseM, shape.RiseM / shape.HalfVm, nullptr, 0.0, false, false);
    add(deckM + shape.RiseM, -shape.RiseM / shape.HalfVm, 0.0, nullptr, true, false);
  } else if (shape.Roof == RoofKind::Mansard) {
    constexpr double sameLineM = 0.001;
    const double b = shape.BreakFracV * shape.HalfVm;
    const double outer = shape.BreakRiseM / std::max(shape.HalfVm - b, sameLineM);
    const double inner = (shape.RiseM - shape.BreakRiseM) / std::max(b, sameLineM);
    add(deckM + outer * shape.HalfVm, outer, nullptr, -b, false, true);
    if (b > 0.0) {
      add(deckM + shape.BreakRiseM + inner * b, inner, -b, 0.0, false, false);
      add(deckM + shape.BreakRiseM + inner * b, -inner, 0.0, b, true, false);
    }
    add(deckM + outer * shape.HalfVm, -outer, b, nullptr, true, false);
  } else {
    return nullptr;
  }
  return bands;
}

inline SurfaceJson InputHip(const Generators::BuildingShape &shape) {
  if (!shape.HipRoof) { return {{"available", false}, {"reason", "no cached native hip plan"}}; }
  if (!*shape.HipRoof) {
    return {{"available", false}, {"native_error", static_cast<unsigned>(shape.HipRoof->error())}};
  }
  const auto &plan = **shape.HipRoof;
  auto facets = SurfaceJson::array();
  for (const auto &facet : plan.Facets()) {
    const double scale = shape.RiseM / plan.MaximumDistanceM();
    const Vec3 normal{{-scale * facet.Gradient.EastM, -scale * facet.Gradient.NorthM, 1}};
    facets.push_back({{"ring", InputRing(facet.Ring)},
                      {"gradient", InputPoint(facet.Gradient)},
                      {"intercept_m", facet.InterceptM},
                      {"source_ring", facet.SourceRing},
                      {"source_edge", facet.SourceEdge},
                      {"normal_enu",
                       InputVector(normal * (1.0 / std::hypot(normal[0], normal[1], normal[2])))}});
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
  result["roof_bands"] = InputRoofBands(shape, deckM);
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
    const auto v = shape.AxisV();
    const double b = shape.BreakFracV * shape.HalfVm;
    for (const double at : {-shape.HalfVm, -b, -b * 0.5, 0.0, b * 0.5, b, shape.HalfVm}) {
      sample({.EastM = shape.Centre.EastM + v.EastM * at,
              .NorthM = shape.Centre.NorthM + v.NorthM * at});
    }
    for (const auto point : shape.Ring) { sample(point); }
    for (const auto &hole : shape.Holes) {
      for (const auto point : hole) { sample(point); }
    }
  }
  return result;
}

}

#endif
