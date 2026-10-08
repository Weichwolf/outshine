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
  const std::array<std::string, 3> layers{"water_polygons", "water_lines", "landuse"};
  OsmField source(6, layers);
  const std::array<OsmField::Declared, 4> features{
      OsmField::Declared{.Layer = "landuse",
                         .Key = "kind",
                         .Value = "forest",
                         .Area = true,
                         .LatLon = {20, 20, 20, 21, 21, 21, 21, 20}},
      OsmField::Declared{.Layer = "water_polygons",
                         .Key = "kind",
                         .Value = "lake",
                         .Area = true,
                         .LatLon = {0, 0, 0, 1, 1, 1, 1, 0}},
      OsmField::Declared{.Layer = "landuse",
                         .Key = "kind",
                         .Value = "forest",
                         .Area = true,
                         .LatLon = {30, 30, 30, 31, 31, 31, 31, 30}},
      OsmField::Declared{.Layer = "water_lines",
                         .Key = "kind",
                         .Value = "river",
                         .LatLon = {0.2, 0.2, 0.2, 0.5, 0.2, 0.8}}};
  source.Declare(features, Ground::TileAt{.X = 32, .Y = 32});
  Ground::VegetationTemplates rules;
  Heights ground;
  WaterField water;
  CHECK(!water.Asset(source).EncodeNative(4096),
        "unfinished water never becomes a ready native asset");
  for (int step = 0; step < 32 && !water.Ingested(source); ++step) {
    (void)water.Ingest(ground, source, rules);
  }
  CHECK(water.Ingested(source) && ground.Queries > 0 && water.Surfaces().size() == 1 &&
            water.Courses().size() == 1,
        "cold generation prepares both standing water and a terrain-dependent flow profile");
  const auto prepared = water.Asset(source);
  CHECK(prepared.Points().size() == 14 && prepared.Points().size() < source.Points().size(),
        "native water owns only its seven contour vertices and excludes unrelated polygons");
  CHECK(water.Asset(source).Points().data() == prepared.Points().data(),
        "unchanged generation reuses the same prepared query coordinates");
  CHECK(prepared.RingsOf(prepared.Surfaces().front()).front().FirstPoint == 0 &&
            water.RingsOf(water.Surfaces().front()).front().FirstPoint == 4 &&
            prepared.Courses().front().FirstPoint == 4 && water.Courses().front().FirstPoint == 12,
        "sparse source ranges compact without changing polygon or flow vertex order");
  const auto bytes = prepared.EncodeNative(4096);
  CHECK(bytes && !prepared.EncodeNative(16), "encoding obeys the package budget");
  if (!bytes || water.Surfaces().empty() || water.Courses().empty()) { return Report(); }
  auto replay = Generators::WaterAsset::DecodeNative(*bytes);
  CHECK(replay && replay->Complete(),
        "native water owns complete query data without a source watermark");
  if (!replay) { return Report(); }
  ground.Allowed = false;
  ground.Queries = 0;
  const LongitudeLatitude lake{.LongitudeDeg = 0.5, .LatitudeDeg = 0.5};
  CHECK(replay->LevelAt(lake) == water.Surfaces().front().LevelM &&
            Generators::WaterAsset(*replay).LevelAt(lake) == replay->LevelAt(lake) &&
            !replay->LevelAt({.LongitudeDeg = 10, .LatitudeDeg = 10}),
        "cache and immutable snapshots answer physical water levels without terrain IO");
  source.Declare({}, Ground::TileAt{.X = 32, .Y = 32});
  CHECK(replay->LevelAt(lake) == water.Surfaces().front().LevelM,
        "native water remains queryable after all mutable source geometry is replaced");
  CHECK(ground.Queries == 0 && replay->Levels() == water.Levels() &&
            replay->Surfaces().front().LevelM == water.Surfaces().front().LevelM &&
            replay->Courses().front().HalfWidthM == water.Courses().front().HalfWidthM &&
            replay->Courses().front().PointCount == water.Courses().front().PointCount &&
            replay->RingsOf(replay->Surfaces().front()).front().PointCount == 4 &&
            replay->OfTile(0).size() == water.OfTile(0).size(),
        "warm water retains exact levels, contours, flow width and membership without terrain IO");
  for (size_t length = 0; length < bytes->size(); ++length) {
    CHECK(!Generators::WaterAsset::DecodeNative(std::span(*bytes).first(length)),
          "every truncated native package is rejected");
  }
  auto damaged = *bytes;
  damaged.front() ^= 1u;
  CHECK(!Generators::WaterAsset::DecodeNative(damaged),
        "unknown native water versions are rejected");
  damaged = *bytes;
  damaged.push_back(0);
  CHECK(!Generators::WaterAsset::DecodeNative(damaged), "trailing native bytes are rejected");
  CHECK(prepared.Points().data() != source.Points().data() &&
            Generators::WaterAsset(prepared).Points().data() == prepared.Points().data(),
        "native query copies share immutable owned coordinates rather than source buffers");
  return Report();
}
