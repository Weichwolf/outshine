#include "Check.h"
#include "ClippedWaterFixture.h"

int main() {
  using namespace outshine::Test;
  using namespace outshine::Test::Water;
  using namespace outshine::Generators::Osm;
  const std::array<std::string, 1> layers{"water_polygons"};
  for (const bool marine : {false, true}) {
    for (const bool reverse : {false, true}) {
      OsmField field(2, layers);
      const auto left = VectorTile(42, "lake", Rectangle(20, 10, 64, 30));
      const auto right = VectorTile(43, marine ? "ocean" : "lake", Rectangle(0, 10, 30, 30));
      const auto first = reverse ? field.Accept(2, 1, right) : field.Accept(1, 1, left);
      const auto second = reverse ? field.Accept(1, 1, left) : field.Accept(2, 1, right);
      CHECK(first && second, "distinct source objects share a complete open boundary edge");
      WaterField water;
      Finish(water, field);
      CHECK(water.Ingested(field) && water.Surfaces().size() == 2,
            "both connected source surfaces remain resident");
      if (water.Surfaces().size() == 2) {
        const float expected = marine ? 0.0f : 10.0f;
        CHECK(water.Surfaces()[0].LevelM == expected && water.Surfaces()[1].LevelM == expected,
              "an open connection shares a level and an identified ocean fixes mean sea level");
      }
    }
  }
  return Report();
}
