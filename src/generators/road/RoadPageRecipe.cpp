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
    product.Parts.push_back({.Kind = RoadPagePartKind::Interval, .Index = interval.Edge});
    const auto before = product.Mesh.Index.size();
    const auto stats = mesher.Sweep(interval.Stations, interval.Surface, product.Mesh);
    product.Swept += stats;
    if (stats.Refused != 0 || product.Mesh.Index.size() == before) {
      product.Diagnostics.push_back(
          {.Part = {.Kind = RoadPagePartKind::Interval, .Index = interval.Edge},
           .Source = interval.Lane,
           .Stats = stats,
           .Empty = product.Mesh.Index.size() == before});
    }
  }
  for (const auto &junction : recipe.Junctions) {
    if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
    product.Parts.push_back({.Kind = RoadPagePartKind::Junction, .Index = junction.Index});
    const auto before = product.Mesh.Index.size();
    mesher.Junction(junction.Gates, junction.Plane, junction.WearsLinear, product.Mesh);
    if (product.Mesh.Index.size() == before) {
      product.Diagnostics.push_back(
          {.Part = {.Kind = RoadPagePartKind::Junction, .Index = junction.Index},
           .Source = junction.Node,
           .Stats = {},
           .Empty = true});
    }
  }
  if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
  return product;
}
}
