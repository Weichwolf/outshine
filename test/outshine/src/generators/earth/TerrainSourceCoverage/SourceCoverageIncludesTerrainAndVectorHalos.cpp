#include "TerrainSourceCoverage.h"
#include "OsmField.h"
#include "OsmLayer.h"
#include "TileGeodesy.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <tuple>
#include <vector>

using namespace outshine;

namespace {
using Key = std::tuple<int, uint32_t, uint32_t>;

Key KeyOf(Data::TileId tile) {
  return {tile.Zoom, tile.X, tile.Y};
}

std::set<Key> NeighbourOracle(Data::TileId tile) {
  std::set<Key> result;
  const auto side = static_cast<int64_t>(uint64_t{1} << static_cast<unsigned>(tile.Zoom));
  for (int64_t row = static_cast<int64_t>(tile.Y) - 1; row <= static_cast<int64_t>(tile.Y) + 1;
       ++row) {
    if (row < 0 || row >= side) { continue; }
    for (int64_t col = static_cast<int64_t>(tile.X) - 1; col <= static_cast<int64_t>(tile.X) + 1;
         ++col) {
      result.emplace(
          tile.Zoom, static_cast<uint32_t>((col + side) % side), static_cast<uint32_t>(row));
    }
  }
  return result;
}

std::set<Key> Keys(std::span<const Data::TileId> tiles) {
  std::set<Key> result;
  for (const auto tile : tiles) { result.insert(KeyOf(tile)); }
  return result;
}

void CheckVectorSourceCoverage() {
  ::outshine::Generators::Osm::OsmField vectors(
      2,
      ::outshine::Generators::Osm::OsmLayerNames(
          {::outshine::Generators::Osm::OsmLayer::Buildings}));
  const auto northWest = Ground::TileFracToGeo({.X = 8.25, .Y = 8.25}, 4);
  const auto southEast = Ground::TileFracToGeo({.X = 9.75, .Y = 9.75}, 4);
  std::array<::outshine::Generators::Osm::OsmField::Declared, 1> buildings{
      {{.Layer = "buildings",
        .Key = {},
        .Value = {},
        .Area = true,
        .LatLon = {northWest.LatitudeDeg,
                   northWest.LongitudeDeg,
                   northWest.LatitudeDeg,
                   southEast.LongitudeDeg,
                   southEast.LatitudeDeg,
                   southEast.LongitudeDeg,
                   southEast.LatitudeDeg,
                   northWest.LongitudeDeg}}}};
  vectors.Declare(buildings, Ground::TileAt{.X = 2, .Y = 2});
  const auto plan = Generators::PlanTerrainSourceTiles(
      {}, {.FinestZoom = 4, .Vectors = &vectors, .AdditionalTiles = {}, .MaximumTiles = 9});
  CHECK(plan && Keys(*plan) == NeighbourOracle({.Zoom = 2, .X = 2, .Y = 2}),
        "a distant building keeps its vector source halo without preloading fine height fields");
}

}

int main() {
  using namespace outshine::Test;
  for (const auto tile : {Data::TileId{.Zoom = 0, .X = 0, .Y = 0},
                          {.Zoom = 2, .X = 0, .Y = 0},
                          {.Zoom = 2, .X = 3, .Y = 3},
                          {.Zoom = 6, .X = 20, .Y = 20}}) {
    const std::array sources{tile, tile};
    const auto plan = Generators::PlanTerrainSourceTiles(
        sources, {.FinestZoom = tile.Zoom, .AdditionalTiles = {}});
    CHECK(plan && Keys(*plan) == NeighbourOracle(tile),
          "source halos wrap longitude, clip poles and deduplicate repeated inputs");
    CHECK(plan && std::ranges::is_sorted(*plan, {}, KeyOf), "source output has a canonical order");
  }
  const std::array<Data::TileId, 1> fine{{{.Zoom = 6, .X = 20, .Y = 20}}};
  auto expected = NeighbourOracle(fine.front());
  const auto parent = NeighbourOracle({.Zoom = 4, .X = 5, .Y = 5});
  expected.insert(parent.begin(), parent.end());
  const auto plan = Generators::PlanTerrainSourceTiles(
      fine, {.FinestZoom = 6, .GroundZoom = 4, .AdditionalTiles = {}});
  CHECK(plan && Keys(*plan) == expected && plan->size() == 18,
        "ground sampling parents have their own complete source halo");
  CHECK(!Generators::PlanTerrainSourceTiles(
            fine, {.FinestZoom = 6, .GroundZoom = 4, .AdditionalTiles = {}, .MaximumTiles = 17}),
        "one missing tile cannot be hidden by truncating the requested coverage");
  const std::array extras{fine.front(), Data::TileId{.Zoom = 6, .X = 1, .Y = 1}};
  const auto extended = Generators::PlanTerrainSourceTiles(
      fine, {.FinestZoom = 6, .GroundZoom = 4, .AdditionalTiles = extras, .MaximumTiles = 19});
  CHECK(extended && extended->size() == 19 && Keys(*extended).contains(KeyOf(extras.back())),
        "route tiles outside camera coverage remain explicit without duplicate budget cost");
  const std::array wrong{Data::TileId{.Zoom = 5, .X = 1, .Y = 1}};
  CHECK(!Generators::PlanTerrainSourceTiles(fine, {.FinestZoom = 6, .AdditionalTiles = wrong}),
        "route source tiles cannot silently change source resolution");
  for (const auto invalid : {Data::TileId{.Zoom = -1, .X = 0, .Y = 0},
                             {.Zoom = 31, .X = 0, .Y = 0},
                             {.Zoom = 2, .X = 4, .Y = 0},
                             {.Zoom = 2, .X = 0, .Y = 4}}) {
    const std::array input{invalid};
    CHECK(!Generators::PlanTerrainSourceTiles(input, {.FinestZoom = 30, .AdditionalTiles = {}}),
          "invalid native source addresses are refused before shifting or enumeration");
  }
  CHECK(!Generators::PlanTerrainSourceTiles(fine, {.FinestZoom = 5, .AdditionalTiles = {}}) &&
            !Generators::PlanTerrainSourceTiles(
                fine, {.FinestZoom = 6, .GroundZoom = 7, .AdditionalTiles = {}}) &&
            !Generators::PlanTerrainSourceTiles(
                fine, {.FinestZoom = 6, .AdditionalTiles = {}, .MaximumTiles = 0}) &&
            !Generators::PlanTerrainSourceTiles(
                fine, {.FinestZoom = 6, .AdditionalTiles = {}, .MaximumTiles = 8193}),
        "source grids and allocation budgets have explicit finite limits");

  CheckVectorSourceCoverage();
  return Report();
}
