#include "WaterField.h"
#include "TerrainLoader.h"
#include "Check.h"
#include <array>
#include <string>

namespace {
class Heights final : public outshine::GroundQuery {
public:
  double Height = 10;

  outshine::GroundSample At(outshine::LongitudeLatitude) const override {
    return outshine::GroundSample::At(Height);
  }

  outshine::GroundSample Resident(outshine::LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock BlockAt(outshine::Ground::TileSpot) const override {
    return outshine::Ground::GroundBlock::Waiting();
  }

  double PostM(double) const override { return 1; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"water_polygons"};
  const std::array<OsmField::Declared, 1> polygons{{{.Layer = "water_polygons",
                                                     .Key = "kind",
                                                     .Value = "lake",
                                                     .Area = true,
                                                     .LatLon = {0, 0, 0, 1, 1, 1, 1, 0}}}};
  OsmField first(6, layers), second(6, layers);
  first.Declare(polygons, {.X = 32, .Y = 32});
  second.Declare(polygons, {.X = 32, .Y = 32});
  CHECK(first.Generation() == second.Generation() && first.OriginToken() != second.OriginToken(),
        "different inputs may have equal local generation counters and shape sizes");
  Heights ground;
  WaterField producer;
  Ground::VegetationTemplates rules;
  for (int at = 0; at < 32 && !producer.Ingested(first); ++at) {
    (void)producer.Ingest(ground, first, rules);
  }
  const auto previous = producer.Asset(first);
  ground.Height = 20;
  (void)producer.Ingest(ground, second, rules);
  for (int at = 0; at < 32 && !producer.Ingested(second); ++at) {
    (void)producer.Ingest(ground, second, rules);
  }
  const auto current = producer.Asset(second);
  const LongitudeLatitude inside{.LongitudeDeg = 0.5, .LatitudeDeg = 0.5};
  CHECK(previous.LevelAt(inside) == 10 && current.LevelAt(inside) == 20,
        "new input ownership regenerates water while the published old asset remains unchanged");
  CHECK(previous.Points().data() != current.Points().data(),
        "independent generations own independent native coordinates");
  return Report();
}
