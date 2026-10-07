#include "PreparedGroundRegions.h"
#include "OsmField.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "TilePool.h"
#include "TerrainLoader.h"
#include "Check.h"
#include "test/outshine/src/generators/osm/MvtLayer/WireFixture.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

namespace {
class NoHeights final : public outshine::GroundQuery {
public:
  outshine::GroundSample At(outshine::LongitudeLatitude) const override {
    return outshine::GroundSample::Waiting();
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
  using namespace outshine::Test;
  using namespace outshine::Test::Mvt;
  using Generators::Osm::OsmField;
  using Generators::Osm::PreparedGroundRegions;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-region-binding-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  const auto opened = PreparedGroundRegions::Open(root.string(), sources, "fixture-rules");
  CHECK(opened, "native regions open before any source or street preparation");
  if (!opened) { return Report(); }
  auto cache = *opened;
  const std::array<std::string, 1> layers{"x"};
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  Append(layer, 0x12, Bytes{0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2});
  Bytes tile;
  Append(tile, 0x1a, layer);
  OsmField vectors(2, layers);
  CHECK(vectors.Accept(1, 1, tile), "source bytes publish their own content digest");
  const Ground::ShapedGround shape;
  const std::array<uint8_t, 3> parameters{1, 2, 3};
  const auto key = cache->Key(vectors, shape, parameters);
  CHECK(!key.empty() && key == cache->Key(vectors, shape, parameters),
        "unchanged source bindings identify a region without building a street field");
  GroundRegionAsset region;
  region.Anchor = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  const auto surface = region.Surfaces.addSurface("ground", Material{});
  CHECK(surface, "native ground material is declared");
  if (!surface) { return Report(); }
  region.GroundSurface = surface->index();
  const auto part = region.Surfaces.addPart("pavement", *surface);
  const std::array<float, 9> positions{0, 100, 0, 10, 101, 0, 0, 102, 10};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  CHECK(part && region.Surfaces.setPositions(*part, positions) &&
            region.Surfaces.setTriangles(*part, indices),
        "native infrastructure is complete");
  region.Terrain.Sheets = {{.Tile = {.Zoom = 2, .X = 1, .Y = 1},
                            .Nodes = {100, 101, 102, 103},
                            .Side = 2,
                            .Postings = 2}};
  region.Terrain.Tiles = 1;
  Generators::Osm::WaterField water;
  NoHeights ground;
  Ground::VegetationTemplates rules;
  (void)water.Ingest(ground, vectors, rules);
  const auto saved = cache->Store(key, GroundRegionBoundsEcef(region), region, vectors, water);
  CHECK(saved, "complete native region is stored under source and demand bindings");
  const auto hit = cache->Load(key, vectors);
  CHECK(hit && *hit && (**hit).Region.Terrain.Sheets == region.Terrain.Sheets,
        "a region hit loads native contact pages without a prepared street graph");
  auto changedShape = shape;
  changedShape.Seed = 9;
  const auto changed = cache->Key(vectors, changedShape, parameters);
  const auto miss = cache->Load(changed, vectors);
  CHECK(changed != key && miss && !*miss, "changed generation parameters cannot reuse the region");
  const std::array<uint8_t, 3> otherDemand{1, 2, 4};
  CHECK(cache->Key(vectors, shape, otherDemand) != key, "different detail demand remains distinct");
  tile.back() = 4;
  OsmField other(2, layers);
  CHECK(other.Accept(1, 1, tile) && cache->Key(other, shape, parameters) != key,
        "a raw geometry change invalidates the region before derived products exist");
  const auto otherRules = PreparedGroundRegions::Open(root.string(), sources, "other-rules");
  CHECK(otherRules && (*otherRules)->Key(vectors, shape, parameters) != key,
        "generator material and placement rules participate in region identity");
  OsmField declared(2, layers);
  declared.Declare({}, Ground::TileAt{.X = 1, .Y = 1});
  CHECK(cache->Key(declared, shape, parameters).empty(),
        "unbound declared inputs cannot claim persistent source identity");
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "test storage is removed");
  return Report();
}
