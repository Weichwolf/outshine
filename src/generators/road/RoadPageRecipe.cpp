#include "RoadPageRecipe.h"
#include <expected>
#include <stop_token>
#include <string_view>

namespace outshine::Generators {

std::expected<RoadPageGeometry, std::string_view>
MeshRoadPage(const RoadMesher &mesher, const RoadPageRecipe &recipe, const std::stop_token &stop) {
  RoadPageGeometry product;
  for (const auto &interval : recipe.Intervals) {
    if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
    product.Swept += mesher.Sweep(interval.Stations, interval.Surface, product.Mesh);
  }
  for (const auto &junction : recipe.Junctions) {
    if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
    mesher.Junction(junction.Gates, junction.Plane, junction.WearsLinear, product.Mesh);
  }
  if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
  return product;
}
}
