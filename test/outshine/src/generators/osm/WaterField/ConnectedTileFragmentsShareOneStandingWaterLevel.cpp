#include "Check.h"
#include "ClippedWaterFixture.h"

#include <limits>

int main() {
  using namespace outshine::Test;
  using namespace outshine::Test::Water;
  using namespace outshine::Generators::Osm;
  const std::array<std::string, 1> layers{"water_polygons"};
  const auto leftShape = Rectangle(20, 10, 64, 50);
  const auto rightShape = Rectangle(0, 20, 30, 40);
  for (const uint64_t id : {uint64_t{0}, std::numeric_limits<uint64_t>::max()}) {
    for (const bool reverse : {false, true}) {
      OsmField field(2, layers);
      const auto left = VectorTile(id, "dock", leftShape),
                 right = VectorTile(id, "dock", rightShape);
      const auto first = reverse ? field.Accept(2, 1, right) : field.Accept(1, 1, left);
      const auto second = reverse ? field.Accept(1, 1, left) : field.Accept(2, 1, right);
      CHECK(first && second, "neighboring clipped water fragments decode in either arrival order");
      WaterField water;
      Finish(water, field);
      CHECK(water.Ingested(field) && water.Surfaces().size() == 2,
            "both native fragments remain available after component resolution");
      if (water.Surfaces().size() == 2) {
        CHECK(
            water.Surfaces()[0].LevelM == 10 && water.Surfaces()[1].LevelM == 10,
            "connected standing-water fragments use one level from their combined terrain samples");
        CHECK(water.OutlierCount() == 2, "shore diagnostics use the final component level");
      }
      const auto snapshot = water.Asset(field);
      CHECK(snapshot.Surfaces().size() == 2 && snapshot.Complete(),
            "query snapshots retain resolved levels and readiness");
    }
  }
  for (const bool buffered : {false, true}) {
    OsmField field(2, layers);
    const auto left = VectorTile(42, "lake", Rectangle(20, 10, buffered ? 80 : 64, 50));
    const auto right = VectorTile(42, "lake", Rectangle(buffered ? -8 : 0, 20, 30, 40));
    CHECK(field.Accept(1, 1, left) && field.Accept(2, 1, right), "buffered source accepted");
    WaterField water;
    Finish(water, field);
    CHECK(water.Surfaces().size() == 2 && water.Surfaces()[0].LevelM == 10 &&
              water.Surfaces()[1].LevelM == 10,
          "buffer overlap and exact clipping edges both preserve physical connectivity");
  }
  OsmField horizontal(2, layers);
  const auto north = VectorTile(42, "lake", Rectangle(10, 20, 50, 64));
  const auto south = VectorTile(42, "lake", Rectangle(20, 0, 40, 30));
  CHECK(horizontal.Accept(1, 1, north) && horizontal.Accept(1, 2, south),
        "north and south fragments accepted");
  WaterField acrossLatitude;
  Finish(acrossLatitude, horizontal, true);
  CHECK(acrossLatitude.Surfaces().size() == 2 && acrossLatitude.Surfaces()[0].LevelM == 10 &&
            acrossLatitude.Surfaces()[1].LevelM == 10,
        "shared latitude boundaries resolve the same component level");
  return Report();
}
