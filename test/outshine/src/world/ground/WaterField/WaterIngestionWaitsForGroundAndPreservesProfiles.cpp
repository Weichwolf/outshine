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
  mutable size_t Queries = 0;
  mutable size_t IgnoredQueries = 0;

  GroundSample At(LongitudeLatitude at) const override {
    ++Queries;
    if (at.LatitudeDeg >= 50) {
      ++IgnoredQueries;
      return GroundSample::Waiting();
    }
    if (Pending) { return GroundSample::Waiting(); }
    constexpr std::array values{10.0, 12.0, 5.0, 3.0};
    if (at.LatitudeDeg < 0 || at.LatitudeDeg >= 4) { return GroundSample::Missing(); }
    return GroundSample::At(values[static_cast<size_t>(at.LatitudeDeg)]);
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
  water.AnchorAt({{6378137, 0, 0}});
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
  return Report();
}
