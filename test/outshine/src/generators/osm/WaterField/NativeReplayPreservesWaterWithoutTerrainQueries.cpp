#include "WaterField.h"
#include "TerrainLoader.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {
class Heights final : public outshine::GroundQuery {
public:
  mutable size_t Queries = 0;
  bool Allowed = true;

  outshine::GroundSample At(outshine::LongitudeLatitude at) const override {
    ++Queries;
    return Allowed ? outshine::GroundSample::At(10 + at.LongitudeDeg)
                   : outshine::GroundSample::Waiting();
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
  const std::array<std::string, 2> layers{"water_polygons", "water_lines"};
  OsmField source(6, layers);
  const std::array<OsmField::Declared, 2> features{
      OsmField::Declared{.Layer = "water_polygons",
                         .Key = "kind",
                         .Value = "lake",
                         .Area = true,
                         .LatLon = {0, 0, 0, 1, 1, 1, 1, 0}},
      OsmField::Declared{.Layer = "water_lines",
                         .Key = "kind",
                         .Value = "river",
                         .LatLon = {0.2, 0.2, 0.2, 0.5, 0.2, 0.8}}};
  source.Declare(features, Ground::TileAt{.X = 32, .Y = 32});
  Ground::VegetationTemplates rules;
  Heights ground;
  WaterField water;
  CHECK(!water.EncodeNative(source, 4096), "unfinished water never becomes a ready native asset");
  for (int step = 0; step < 32 && !water.Ingested(source); ++step) {
    (void)water.Ingest(ground, source, rules);
  }
  CHECK(water.Ingested(source) && ground.Queries > 0 && water.Surfaces().size() == 1 &&
            water.Courses().size() == 1,
        "cold generation prepares both standing water and a terrain-dependent flow profile");
  const auto bytes = water.EncodeNative(source, 4096);
  CHECK(bytes && !water.EncodeNative(source, 16), "encoding obeys the package budget");
  if (!bytes || water.Surfaces().empty() || water.Courses().empty()) { return Report(); }
  auto replay = WaterField::DecodeNative(*bytes, source);
  CHECK(replay && replay->Ingested(source), "native water restores the complete source watermark");
  if (!replay) { return Report(); }
  ground.Allowed = false;
  ground.Queries = 0;
  (void)replay->Ingest(ground, source, rules);
  CHECK(ground.Queries == 0 && replay->Levels() == water.Levels() &&
            replay->Surfaces().front().LevelM == water.Surfaces().front().LevelM &&
            replay->Courses().front().HalfWidthM == water.Courses().front().HalfWidthM &&
            replay->Courses().front().PointCount == water.Courses().front().PointCount &&
            replay->RingsOf(replay->Surfaces().front()).front().PointCount == 4 &&
            replay->OfTile(0).size() == water.OfTile(0).size(),
        "warm water retains exact levels, contours, flow width and membership without terrain IO");
  for (size_t length = 0; length < bytes->size(); ++length) {
    CHECK(!WaterField::DecodeNative(std::span(*bytes).first(length), source),
          "every truncated native package is rejected");
  }
  auto damaged = *bytes;
  damaged.front() ^= 1u;
  CHECK(!WaterField::DecodeNative(damaged, source), "unknown native water versions are rejected");
  damaged = *bytes;
  damaged.push_back(0);
  CHECK(!WaterField::DecodeNative(damaged, source), "trailing native bytes are rejected");
  OsmField elsewhere(6, layers);
  elsewhere.Declare({}, Ground::TileAt{.X = 32, .Y = 32});
  CHECK(!WaterField::DecodeNative(*bytes, elsewhere),
        "water ranges cannot bind a different source layout");
  return Report();
}
