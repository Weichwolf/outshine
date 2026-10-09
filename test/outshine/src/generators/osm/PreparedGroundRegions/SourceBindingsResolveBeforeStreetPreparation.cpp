#include "PreparedGroundRegions.h"
#include "OsmField.h"
#include "WaterField.h"
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
#include <vector>

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

void FreshRequestHit(const std::string &directory,
                     const outshine::Data::SourceSet &sources,
                     const std::string &key) {
  using namespace outshine;
  using namespace outshine::Test;
  using Generators::Osm::MvtSchema;
  using Generators::Osm::PreparedGroundRegions;
  const auto opened = PreparedGroundRegions::Open(directory, sources, "fixture-rules");
  CHECK(opened, "fresh service opens without vector, elevation or street preparation");
  if (!opened) { return; }
  const Ground::ShapedGround shape;
  const std::array<uint8_t, 3> parameters{1, 2, 3};
  const std::array<Ground::TileSpot, 1> inputs{{{.Zoom = 2, .X = 1, .Y = 1}}};
  const auto request = (*opened)->RequestKey(2, MvtSchema::Shortbread, shape, parameters, inputs);
  auto hit = (*opened)->LoadRequest(request);
  CHECK(hit && *hit && (**hit).Key == key && (**hit).Region.Terrain.Sheets.size() == 1 &&
            (**hit).Region.Terrain.Sheets[0].Nodes == std::vector<float>({100, 101, 102, 103}),
        "bound demand restores native contacts before any unavailable inputs exist");
  const auto changed = (*opened)->RequestKey(3, MvtSchema::Shortbread, shape, parameters, inputs);
  const auto miss = (*opened)->LoadRequest(changed);
  CHECK(changed != request && miss && !*miss, "different region layout remains a miss");
  CHECK((*opened)->RequestKey(2, MvtSchema::OpenMapTiles, shape, parameters, inputs) != request,
        "source schema participates in the source-independent demand");
  const std::array<Ground::TileSpot, 2> expanded{{inputs[0], {.Zoom = 2, .X = 2, .Y = 1}}};
  CHECK((*opened)->RequestKey(2, MvtSchema::Shortbread, shape, parameters, expanded) != request,
        "a partial input layout cannot satisfy the completed region demand");
}
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
  const auto saved =
      cache->Store(key, GroundRegionBoundsEcef(region), region, water.Asset(vectors));
  CHECK(saved, "complete native region is stored under source and demand bindings");
  const auto hit = cache->Load(key);
  CHECK(hit && *hit && (**hit).Region.Terrain.Sheets == region.Terrain.Sheets,
        "a region hit loads native contact pages without a prepared street graph");
  const std::array<Ground::TileSpot, 1> inputs{{{.Zoom = 2, .X = 1, .Y = 1}}};
  const auto request =
      cache->RequestKey(vectors.Zoom(), vectors.Schema(), shape, parameters, inputs);
  CHECK(cache->BindRequest(key, request),
        "legacy region gains a demand without package generation");
  FreshRequestHit(root.string(), sources, key);
  auto changedShape = shape;
  changedShape.Seed = 9;
  const auto changed = cache->Key(vectors, changedShape, parameters);
  const auto miss = cache->Load(changed);
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
