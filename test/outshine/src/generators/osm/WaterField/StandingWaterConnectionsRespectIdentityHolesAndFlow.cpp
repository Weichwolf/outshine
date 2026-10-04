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
    const auto first = field.Accept(1, 1, left), second = field.Accept(2, 1, right);
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
  separate(VectorTile(42, "lake", leftShape), VectorTile(43, "lake", rightShape));
  separate(VectorTile(std::nullopt, "lake", leftShape),
           VectorTile(std::nullopt, "lake", rightShape));
  separate(VectorTile(42, "lake", leftShape), VectorTile(42, "lake", Rectangle(0, 31, 30, 50)));
  separate(VectorTile(42, "lake", leftShape), VectorTile(42, "lake", Rectangle(0, 30, 30, 50)));
  separate(VectorTile(42, "river", leftShape), VectorTile(42, "river", rightShape));
  auto island = Rectangle(55, 5, 75, 35);
  std::ranges::reverse(island);
  separate(VectorTile(42, "lake", Rectangle(20, 0, 80, 40), island),
           VectorTile(42, "lake", rightShape));
  return Report();
}
