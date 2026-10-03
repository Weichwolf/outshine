#include "Check.h"
#include "TerrainLoader.h"
#include "WaterField.h"

#include <array>
#include <cstddef>
#include <span>
#include <string>

using namespace outshine;
using namespace outshine::Generators::Osm;

namespace {
class Heights final : public GroundQuery {
public:
  bool Pending = true;
  mutable size_t Queries = 0;

  GroundSample At(LongitudeLatitude at) const override {
    ++Queries;
    return Pending ? GroundSample::Waiting() : GroundSample::At(100.0 + 10.0 * at.LatitudeDeg);
  }

  GroundSample Resident(LongitudeLatitude at) const override { return At(at); }

  Ground::GroundBlock BlockAt(Ground::TileSpot) const override {
    return Ground::GroundBlock::Waiting();
  }

  double PostM(double) const override { return 1.0; }
};
}

int main() {
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"water_polygons"};
  OsmField field(6, layers);
  const std::array<OsmField::Declared, 3> features{
      OsmField::Declared{.Layer = "water_polygons",
                         .Key = "kind",
                         .Value = "ocean",
                         .Area = true,
                         .LatLon = {1, 0, 1, 1, 2, 1, 2, 0}},
      OsmField::Declared{.Layer = "water_polygons",
                         .Key = "kind",
                         .Value = "ocean",
                         .Area = true,
                         .LatLon = {-20, 0, -20, 1, -19, 1, -19, 0}},
      OsmField::Declared{.Layer = "water_polygons",
                         .Key = "kind",
                         .Value = "lake",
                         .Area = true,
                         .LatLon = {3, 0, 3, 1, 4, 1, 4, 0}}};
  field.Declare(std::span(features).first(2), Ground::TileAt{.X = 32, .Y = 32});
  Heights ground;
  VegetationTemplates materials;
  WaterField water;
  CHECK(water.Ingest(ground, field, materials) == 2 && water.Ingested(field),
        "known ocean surfaces do not wait for unrelated terrain acquisition");
  CHECK(ground.Queries == 0, "a mean ocean level requires no terrain sampling");
  if (water.Surfaces().size() == 2) {
    CHECK(water.Surfaces()[0].LevelM == 0 && water.Surfaces()[1].LevelM == 0,
          "separate coastal polygons share mean sea level across positive and negative shores");
  }
  ground.Pending = false;
  field.Declare(features, Ground::TileAt{.X = 32, .Y = 32});
  CHECK(water.Ingest(ground, field, materials) == 3 && water.Ingested(field),
        "source changes rebuild native surfaces without dropping the inland lake");
  if (water.Surfaces().size() == 3) {
    CHECK(water.Surfaces()[0].LevelM == 0 && water.Surfaces()[1].LevelM == 0 &&
              water.Surfaces()[2].LevelM == 130,
          "marine datum does not flatten the independently elevated inland lake");
  }
  CHECK(ground.Queries == 4, "only the lake samples its shoreline terrain");
  return Report();
}
