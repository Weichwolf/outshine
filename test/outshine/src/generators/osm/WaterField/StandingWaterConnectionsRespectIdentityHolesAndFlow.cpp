#include <string>
#include <optional>
#include <array>
#include <algorithm>
#include "Check.h"
#include "ClippedWaterFixture.h"

int main() {
  using namespace outshine::Test;
  using namespace outshine::Test::Water;
  using namespace outshine::Generators::Osm;
  const std::array<std::string, 1> layers{"water_polygons"};
  const auto leftShape = Rectangle(20, 10, 64, 30);
  const auto rightShape = Rectangle(0, 10, 30, 30);
  const auto separate = [&](const Bytes &left, const Bytes &right) {
    OsmField field(2, layers);
    const auto first = field.Accept(1, 1, left);
    const auto second = field.Accept(2, 1, right);
    CHECK(first && second, "source polygons accepted");
    WaterField water;
    Finish(water, field);
    CHECK(water.Ingested(field) && water.Surfaces().size() == 2,
          "independent source surfaces remain resident");
    if (water.Surfaces().size() == 2) {
      CHECK(water.Surfaces()[0].LevelM == 10 && water.Surfaces()[1].LevelM == 100,
            "source identity alone does not flatten separate components or flowing water");
    }
  };
  separate(VectorTile(42, "lake", {.Exterior = leftShape, .Hole = {}}),
           VectorTile(43, "lake", {.Exterior = Rectangle(0, 31, 30, 50), .Hole = {}}));
  separate(VectorTile(std::nullopt, "lake", {.Exterior = leftShape, .Hole = {}}),
           VectorTile(std::nullopt, "lake", {.Exterior = rightShape, .Hole = {}}));
  separate(VectorTile(42, "lake", {.Exterior = leftShape, .Hole = {}}),
           VectorTile(42, "lake", {.Exterior = Rectangle(0, 31, 30, 50), .Hole = {}}));
  separate(VectorTile(42, "lake", {.Exterior = leftShape, .Hole = {}}),
           VectorTile(42, "lake", {.Exterior = Rectangle(0, 30, 30, 50), .Hole = {}}));
  separate(VectorTile(42, "river", {.Exterior = leftShape, .Hole = {}}),
           VectorTile(42, "river", {.Exterior = rightShape, .Hole = {}}));
  auto island = Rectangle(55, 5, 75, 35);
  std::ranges::reverse(island);
  separate(VectorTile(42, "lake", {.Exterior = Rectangle(20, 0, 80, 40), .Hole = island}),
           VectorTile(42, "lake", {.Exterior = rightShape, .Hole = {}}));
  return Report();
}
