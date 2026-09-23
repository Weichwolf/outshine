#include "WaterField.h"
#include "TerrainLoader.h"
#include "Check.h"
#include <array>

using namespace outshine;
using namespace outshine::Ground;

namespace {
class Heights final : public GroundQuery {
public:
  bool Pending = true;
  mutable bool DelayThird = false;
  mutable size_t Queries = 0;
  mutable size_t IgnoredQueries = 0;
  mutable std::array<size_t, 4> Hits{};

  GroundSample At(LongitudeLatitude at) const override {
    ++Queries;
    if (at.LatitudeDeg >= 50) {
      ++IgnoredQueries;
      return GroundSample::Waiting();
    }
    if (Pending) { return GroundSample::Waiting(); }
    constexpr std::array values{10.0, 12.0, 5.0, 3.0};
    if (at.LatitudeDeg < 0 || at.LatitudeDeg >= 4) { return GroundSample::Missing(); }
    const size_t index = static_cast<size_t>(at.LatitudeDeg);
    ++Hits[index];
    if (index == 2 && DelayThird) {
      DelayThird = false;
      return GroundSample::Waiting();
    }
    return GroundSample::At(values[index]);
  }

  GroundSample Resident(LongitudeLatitude at) const override { return At(at); }

  GroundBlock BlockAt(TileSpot) const override { return GroundBlock::Waiting(); }

  double PostM(double) const override { return 1; }
};
}

int main() {
  using namespace outshine::Test;
  const std::array<std::string, 3> layers{"water_polygons", "water_lines", "buildings"};
  OsmField field(6, layers);
  std::vector<OsmField::Declared> features{
      {.Layer = "water_lines", .Key = "kind", .Value = "river", .LatLon = {0, 0, 1, 0, 2, 0, 3, 0}},
      {.Layer = "water_lines", .Key = "kind", .Value = "river", .LatLon = {3, 1, 2, 1, 1, 1, 0, 1}},
      {.Layer = "water_polygons",
       .Key = "kind",
       .Value = "lake",
       .Area = true,
       .LatLon = {0, 0, 1, 1, 2, 1, 3, 0}},
      {.Layer = "water_polygons",
       .Key = "kind",
       .Value = "lake",
       .Area = true,
       .LatLon = {4, 0, 0, 1, 1, 0}},
      {.Layer = "water_lines",
       .Key = "kind",
       .Value = "river",
       .Tunnel = true,
       .LatLon = {50, 0, 51, 0}},
      {.Layer = "water_polygons",
       .Key = "kind",
       .Value = "lake",
       .Area = true,
       .LatLon = {50, 0, 51, 0}},
      {.Layer = "buildings",
       .Key = "kind",
       .Value = "building",
       .Area = true,
       .LatLon = {50, 0, 51, 1, 50, 1}},
      {.Layer = "water_lines",
       .Key = "kind",
       .Value = "river",
       .LatLon = std::vector<double>(513 * 2, 50.0)}};
  field.Declare(features, TileAt{.X = 32, .Y = 32});
  VegetationTemplates materials;
  Heights ground;
  WaterField water;
  CHECK(water.Ingest(ground, field, materials) == 0 && water.IngestedTiles() == 0 &&
            water.Courses().empty() && water.Levels().empty(),
        "pending terrain publishes no partial water tile");
  CHECK(!water.Ingested(field) && water.Deferrals() == 1, "pending tile remains retryable");
  ground.Pending = false;
  CHECK(water.Ingest(ground, field, materials) == 1 && water.Ingested(field) &&
            water.IngestedTiles() == 1,
        "resolved tile publishes once");
  CHECK(water.Courses().size() == 2 && water.Surfaces().size() == 1,
        "line and polygon products remain separate");
  CHECK(water.Levels() == std::vector<float>({10, 10, 5, 3, 3, 5, 10, 10}),
        "both point orders preserve monotone downstream levels");
  if (water.Courses().size() == 2) {
    CHECK(water.Courses()[0].FirstLevel == 0 && water.Courses()[1].FirstLevel == 4 &&
              water.Courses()[0].PointCount == 4 && water.Courses()[1].HalfWidthM == 1,
          "course spans and fallback half width remain valid");
  }
  CHECK(water.NoGroundCount() == 1 && water.OutlierCount() == 1,
        "missing height and high shoreline are distinct diagnostics");
  if (!water.Surfaces().empty()) {
    CHECK(water.Surfaces()[0].LevelM == 3, "declared lower percentile sets lake level");
  }
  CHECK(ground.IgnoredQueries == 0, "ignored rings never request terrain");
  const auto queries = ground.Queries;
  CHECK(water.Ingest(ground, field, materials) == 1 && ground.Queries == queries &&
            water.IngestedTiles() == 1,
        "completed tile is not queried or appended twice");
  Heights delayed;
  delayed.Pending = false;
  delayed.DelayThird = true;
  WaterField resumed;
  CHECK(resumed.Ingest(delayed, field, materials) == 0 && !resumed.Ingested(field) &&
            resumed.Courses().empty() && resumed.Surfaces().empty() && delayed.Hits[0] == 1 &&
            delayed.Hits[1] == 1 && delayed.Hits[2] == 1,
        "mid-ring pending retains only staged heights and publishes nothing");
  CHECK(resumed.Ingest(delayed, field, materials) == 1 && resumed.Ingested(field) &&
            delayed.Hits[0] == ground.Hits[0] && delayed.Hits[1] == ground.Hits[1] &&
            delayed.Hits[2] == ground.Hits[2] + 1,
        "resume repeats only pending point and later occurrences");
  CHECK(resumed.Courses().size() == water.Courses().size() && resumed.Levels() == water.Levels() &&
            resumed.Surfaces().size() == water.Surfaces().size() &&
            resumed.Surfaces()[0].LevelM == water.Surfaces()[0].LevelM,
        "paced water admission preserves complete profile and level");
  Heights revised;
  revised.Pending = false;
  revised.DelayThird = true;
  WaterField revisionWater;
  CHECK(revisionWater.Ingest(revised, field, materials) == 0 && revised.Hits[0] == 1,
        "old source revision holds incomplete water candidate");
  const uint64_t oldGeneration = field.Generation();
  features[6].Value = "house";
  field.Declare(features, TileAt{.X = 32, .Y = 32});
  CHECK(field.Generation() == oldGeneration + 1, "declared OSM revision advances generation");
  const auto revisionSurfaces = revisionWater.Ingest(revised, field, materials);
  CHECK(revisionSurfaces == 1, "new source revision publishes complete water tile");
  CHECK_NEAR(revised.Hits[0],
             ground.Hits[0] + 1,
             0,
             "queries",
             "new source revision discards staged height queries");
  CHECK(revisionWater.Levels() == water.Levels(), "new source revision preserves complete profile");
  OsmField longField(6, layers);
  std::vector<double> longLine;
  for (int point = 0; point < 130; ++point) {
    longLine.push_back(0.0);
    longLine.push_back(static_cast<double>(point));
  }
  const std::array longFeatures{OsmField::Declared{
      .Layer = "water_lines", .Key = "kind", .Value = "river", .LatLon = longLine}};
  longField.Declare(longFeatures, TileAt{.X = 32, .Y = 32});
  Heights longGround;
  longGround.Pending = false;
  WaterField longWater;
  (void)longWater.Ingest(longGround, longField, materials);
  CHECK(!longWater.Ingested(longField) && longGround.Queries <= 128 && longWater.Courses().empty(),
        "large ring stops at admission step cap without partial publication");
  for (int advance = 0; advance < 10 && !longWater.Ingested(longField); ++advance) {
    (void)longWater.Ingest(longGround, longField, materials);
  }
  CHECK(longWater.Ingested(longField) && longGround.Queries == 130 &&
            longWater.Courses().size() == 1 && longWater.Levels().size() == 130,
        "large ring resumes without repeating resolved points");
  return Report();
}
