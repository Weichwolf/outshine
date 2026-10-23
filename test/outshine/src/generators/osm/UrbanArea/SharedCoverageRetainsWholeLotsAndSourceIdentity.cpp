#include "UrbanArea.h"
#include "UrbanAreaCodec.h"
#include "UrbanAreaDomain.h"
#include "OsmField.h"
#include "Check.h"
#include "TileGeodesy.h"
#include "math/Vec2.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
namespace Osm = outshine::Generators::Osm;
constexpr int kZoom = 16;
constexpr double kPitchM = 32;
const std::array<std::string, 2> kLayers{"sites", "water_polygons"};
enum class Shape { Full, Hole, Narrow, Setback };

Osm::OsmField::Declared
Area(Ground::TileAt tile, const std::string &layer, std::array<double, 4> bounds) {
  const double span = Data::kMercatorGirthM / 65536;
  const std::array<Vec2, 4> points{{{{bounds[0], bounds[1]}},
                                    {{bounds[2], bounds[1]}},
                                    {{bounds[2], bounds[3]}},
                                    {{bounds[0], bounds[3]}}}};
  std::vector<double> coordinates;
  for (const auto &point : points) {
    const auto geo = Ground::TileFracToGeo(
        {.X = tile.X + point[0] / span, .Y = tile.Y - point[1] / span}, kZoom);
    coordinates.push_back(geo.LatitudeDeg);
    coordinates.push_back(geo.LongitudeDeg);
  }
  return {
      .Layer = layer, .Key = "kind", .Value = "residential", .Area = true, .LatLon = coordinates};
}

std::vector<Osm::OsmField::Declared> Sources(Ground::TileAt tile, Shape shape) {
  const double span = Data::kMercatorGirthM / 65536;
  const double left = tile.X * span - Data::kMercatorGirthM / 2;
  const double top = Data::kMercatorGirthM / 2 - tile.Y * span;
  const double x = (std::floor((left + span / 2) / kPitchM) + .5) * kPitchM - left;
  const double y = (std::floor((top - span / 2) / kPitchM) + .5) * kPitchM - top;
  constexpr double guardM = 500;
  std::array<double, 4> urban{-guardM, -span - guardM, span + guardM, guardM};
  if (shape == Shape::Narrow) { urban = {x - 5, -span - guardM, x + 5, guardM}; }
  if (shape == Shape::Setback) {
    constexpr double blockM = kPitchM * 8;
    const double bx = std::floor((left + span / 2) / blockM) * blockM - left;
    const double by = std::floor((top - span / 2) / blockM) * blockM - top;
    constexpr double setbackM = 3;
    urban = {bx + setbackM, by + setbackM, bx + blockM - setbackM, by + blockM - setbackM};
  }
  std::vector result{Area(tile, "sites", urban)};
  if (shape == Shape::Hole) {
    result.push_back(Area(tile, "water_polygons", {x - .5, y - .5, x + .5, y + .5}));
  }
  return result;
}

void CheckFootprints(const Osm::UrbanArea &area, const Osm::UrbanDetail::Domain &domain) {
  bool covered = true;
  const double half = area.Recipe.ProjectedPitchM * area.Recipe.FillFraction / 2;
  for (uint32_t row = 0; row < area.Rows; ++row) {
    for (uint32_t column = 0; column < area.Columns; ++column) {
      if (area.Tiers[static_cast<size_t>(row) * area.Columns + column] == 0) { continue; }
      const double x =
          (static_cast<double>(area.FirstCell[0] + column) + .5) * area.Recipe.ProjectedPitchM;
      const double y =
          (static_cast<double>(area.FirstCell[1] - row) + .5) * area.Recipe.ProjectedPitchM;
      const auto whole = domain.Covers({x - half, y - half, x + half, y + half});
      if (!whole || !*whole) { covered = false; }
    }
  }
  CHECK(covered, "every accepted whole footprint is covered, including interior holes");
}

void Recipes(const Osm::OsmField &field, const Osm::UrbanDetail::Domain &domain, double fill) {
  const std::array<const Osm::OsmField *, 1> inputs{&field};
  Osm::UrbanAreaRecipe recipe;
  recipe.FillFraction = fill;
  recipe.PatchSide = 1;
  auto reference = Osm::GenerateUrbanArea(inputs, recipe);
  CHECK(reference, "individual-cell reference is generated");
  if (!reference) { return; }
  const auto expected = Osm::EncodeUrbanArea(*reference);
  CHECK(expected, "complete reference product encodes");
  constexpr std::array<uint32_t, 4> sides{1, 3, 8, 32};
  for (const auto side : sides) {
    recipe.PatchSide = side;
    auto actual = Osm::GenerateUrbanArea(inputs, recipe);
    CHECK(actual, "declared intrinsic patch partition generates the same region");
    if (!actual) { continue; }
    CheckFootprints(*actual, domain);
    CHECK(actual->Recipe.PatchSide == side, "intrinsic patch partition remains the requested one");
    actual->Recipe.PatchSide = 1;
    const auto encoded = Osm::EncodeUrbanArea(*actual);
    CHECK(
        expected && encoded && *expected == *encoded,
        "all categorical cells, bounds, source roles and identities remain partition-independent");
  }
}

void Case(Ground::TileAt tile, Shape shape) {
  Osm::OsmField field(kZoom, kLayers);
  const auto sources = Sources(tile, shape);
  field.Declare(sources, tile, "shared-coverage-source");
  CHECK(field.Tiles().size() == 1, "fixture declares one geographically anchored owner");
  if (field.Tiles().size() != 1) { return; }
  const std::array<const Osm::OsmField *, 1> inputs{&field};
  const auto domain =
      Osm::UrbanDetail::BuildDomain(inputs, Osm::UrbanDetail::FrameFor(field.Tiles().front()), {});
  CHECK(domain, "independent full-footprint predicate domain prepares");
  if (!domain) { return; }
  constexpr double smallFillFraction = .01;
  Recipes(field, **domain, smallFillFraction);
  Recipes(field, **domain, Osm::UrbanAreaRecipe::DefaultFillFraction);
  Recipes(field, **domain, std::nextafter(1., 0.));
}
}

int main() {
  constexpr std::array<Ground::TileAt, 4> tiles{{{.X = 32767, .Y = 32768},
                                                 {.X = 4, .Y = 32768},
                                                 {.X = 65531, .Y = 32768},
                                                 {.X = 32768, .Y = 4}}};
  constexpr std::array shapes{Shape::Full, Shape::Hole, Shape::Narrow, Shape::Setback};
  for (const auto tile : tiles) {
    for (const auto shape : shapes) { Case(tile, shape); }
  }
  return outshine::Test::Report();
}
