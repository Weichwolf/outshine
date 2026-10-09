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
  const int64_t side = int64_t{1} << tile.Zoom;
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
}

int main() {
  using namespace outshine::Test;
  for (const auto tile : {Data::TileId{.Zoom = 0, .X = 0, .Y = 0},
                          {.Zoom = 2, .X = 0, .Y = 0},
                          {.Zoom = 2, .X = 3, .Y = 3},
                          {.Zoom = 6, .X = 20, .Y = 20}}) {
    const std::array sources{tile, tile};
    const auto plan = PlanTerrainSourceTiles(sources, {.FinestZoom = tile.Zoom});
    CHECK(plan && Keys(*plan) == NeighbourOracle(tile),
          "source halos wrap longitude, clip poles and deduplicate repeated inputs");
    CHECK(plan && std::ranges::is_sorted(*plan, {}, KeyOf), "source output has a canonical order");
  }
  const std::array<Data::TileId, 1> fine{{{.Zoom = 6, .X = 20, .Y = 20}}};
  auto expected = NeighbourOracle(fine.front());
  const auto parent = NeighbourOracle({.Zoom = 4, .X = 5, .Y = 5});
  expected.insert(parent.begin(), parent.end());
  const auto plan = PlanTerrainSourceTiles(fine, {.FinestZoom = 6, .GroundZoom = 4});
  CHECK(plan && Keys(*plan) == expected && plan->size() == 18,
        "ground sampling parents have their own complete source halo");
  CHECK(!PlanTerrainSourceTiles(fine, {.FinestZoom = 6, .GroundZoom = 4, .MaximumTiles = 17}),
        "one missing tile cannot be hidden by truncating the requested coverage");
  const std::array extras{fine.front(), Data::TileId{.Zoom = 6, .X = 1, .Y = 1}};
  const auto extended = PlanTerrainSourceTiles(
      fine, {.FinestZoom = 6, .GroundZoom = 4, .AdditionalTiles = extras, .MaximumTiles = 19});
  CHECK(extended && extended->size() == 19 && Keys(*extended).contains(KeyOf(extras.back())),
        "route tiles outside camera coverage remain explicit without duplicate budget cost");
  const std::array wrong{Data::TileId{.Zoom = 5, .X = 1, .Y = 1}};
  CHECK(!PlanTerrainSourceTiles(fine, {.FinestZoom = 6, .AdditionalTiles = wrong}),
        "route source tiles cannot silently change source resolution");
  for (const auto invalid : {Data::TileId{.Zoom = -1, .X = 0, .Y = 0},
                             {.Zoom = 31, .X = 0, .Y = 0},
                             {.Zoom = 2, .X = 4, .Y = 0},
                             {.Zoom = 2, .X = 0, .Y = 4}}) {
    const std::array input{invalid};
    CHECK(!PlanTerrainSourceTiles(input, {.FinestZoom = 30}),
          "invalid native source addresses are refused before shifting or enumeration");
  }
  CHECK(!PlanTerrainSourceTiles(fine, {.FinestZoom = 5}) &&
            !PlanTerrainSourceTiles(fine, {.FinestZoom = 6, .GroundZoom = 7}) &&
            !PlanTerrainSourceTiles(fine, {.FinestZoom = 6, .MaximumTiles = 0}) &&
            !PlanTerrainSourceTiles(fine, {.FinestZoom = 6, .MaximumTiles = 8193}),
        "source grids and allocation budgets have explicit finite limits");

  ::outshine::Generators::Osm::OsmField vectors(
      2,
      ::outshine::Generators::Osm::OsmLayerNames(
          {::outshine::Generators::Osm::OsmLayer::Buildings}));
  const auto northWest = Ground::TileFracToGeo({.X = 8.25, .Y = 8.25}, 4);
  const auto southEast = Ground::TileFracToGeo({.X = 9.75, .Y = 9.75}, 4);
  std::array<::outshine::Generators::Osm::OsmField::Declared, 1> buildings{
      {{.Layer = "buildings",
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
  auto buildingExpected = NeighbourOracle({.Zoom = 2, .X = 2, .Y = 2});
  for (uint32_t y = 8; y <= 9; ++y) {
    for (uint32_t x = 8; x <= 9; ++x) { buildingExpected.emplace(4, x, y); }
  }
  const auto buildingsPlan = PlanTerrainSourceTiles({}, {.FinestZoom = 4, .Vectors = &vectors});
  CHECK(buildingsPlan && Keys(*buildingsPlan) == buildingExpected,
        "vector neighbours and building footprints contribute different source resolutions");
  const auto streetsPlan = PlanTerrainSourceTiles(
      {}, {.FinestZoom = 4, .Vectors = &vectors, .BuildingFootprints = false, .MaximumTiles = 9});
  CHECK(streetsPlan && Keys(*streetsPlan) == NeighbourOracle({.Zoom = 2, .X = 2, .Y = 2}),
        "initial street coverage retains source halos without refining every building footprint");
  const auto datelineNorth = Ground::TileFracToGeo({.X = 0.0, .Y = 128.25}, 8);
  const auto datelineSouth = Ground::TileFracToGeo({.X = 0.0, .Y = 129.75}, 8);
  buildings.front().LatLon = {datelineNorth.LatitudeDeg,
                              179.0,
                              datelineNorth.LatitudeDeg,
                              181.0,
                              datelineSouth.LatitudeDeg,
                              181.0,
                              datelineSouth.LatitudeDeg,
                              179.0};
  vectors.Declare(buildings, Ground::TileAt{.X = 3, .Y = 2});
  auto seamExpected = NeighbourOracle({.Zoom = 2, .X = 3, .Y = 2});
  for (const uint32_t x : {0u, 255u}) {
    for (const uint32_t y : {128u, 129u}) { seamExpected.emplace(8, x, y); }
  }
  const auto seam = PlanTerrainSourceTiles({}, {.FinestZoom = 8, .Vectors = &vectors});
  CHECK(seam && Keys(*seam) == seamExpected,
        "buffered buildings crossing 180 degrees request both adjacent world columns");
  for (size_t at = 1; at < buildings.front().LatLon.size(); at += 2) {
    buildings.front().LatLon[at] -= 360.0;
  }
  vectors.Declare(buildings, Ground::TileAt{.X = 3, .Y = 2});
  const auto wrapped = PlanTerrainSourceTiles({}, {.FinestZoom = 8, .Vectors = &vectors});
  CHECK(wrapped && seam && *wrapped == *seam,
        "changing the unwrapped longitude origin cannot change source demand");
  buildings.front().LatLon = {-60, -170, -60, 170, 60, 170, 60, -170};
  vectors.Declare(buildings, Ground::TileAt{.X = 2, .Y = 2});
  CHECK(!PlanTerrainSourceTiles({}, {.FinestZoom = 30, .Vectors = &vectors}),
        "a huge valid footprint is refused before enumerating billions of source tiles");
  return Report();
}
