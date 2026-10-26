#include "RoadPageRecipe.h"
#include "RoadPageMeshing.h"
#include "SurfaceBody.h"
#include <expected>
#include <stop_token>
#include <string_view>

namespace outshine::Generators {
std::expected<RoadPageGeometry, std::string_view>
MeshRoadPage(const RoadMesher &mesher, const RoadPageRecipe &recipe, const std::stop_token &stop) {
  RoadPageGeometry product;
  for (const auto &interval : recipe.Intervals) {
    if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
    const auto appended =
        AppendRoadPageInterval(mesher,
                               {.Edge = interval.Edge,
                                .Source = interval.Lane,
                                .Stations = interval.Stations,
                                .Surface = interval.Surface,
                                .Body = interval.StepBody ? &interval.StepBody->Geometry : nullptr},
                               product);
    if (!appended) { return std::unexpected(appended.error()); }
  }
  for (const auto &junction : recipe.Junctions) {
    if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
    AppendRoadPageJunction(mesher,
                           {.Index = junction.Index,
                            .Node = junction.Node,
                            .Gates = junction.Gates,
                            .Plane = junction.Plane,
                            .WearsLinear = junction.WearsLinear},
                           product);
  }
  if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
  return product;
}
}
