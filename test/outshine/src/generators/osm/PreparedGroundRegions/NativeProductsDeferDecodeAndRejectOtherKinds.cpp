#include "OpenAssetStore.h"
#include "StoreGroundRegion.h"
#include "PreparedGroundRegions.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t kLimit = size_t{1024} * 1024;
  const auto directory = PlantedPath("native-ground-read-products");
  std::filesystem::remove_all(directory);
  const auto store = OpenAssetStore(directory);
  Data::RawSourceCache raw({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  const Data::SourceSet inputs(raw);
  const auto opened = Generators::Osm::PreparedGroundRegions::Open(store, inputs, "native-read");
  CHECK(opened, "a real region decoder and asset store open without provider work");
  if (!opened) { return Report(); }
  auto &regions = **opened;
  GroundRegionAsset region;
  const auto surface = region.Surfaces.addSurface("ground", Material{});
  CHECK(surface, "the fixture supplies a valid native ground material");
  if (!surface) { return Report(); }
  region.GroundSurface = surface->index();
  region.Terrain.Sheets = {{.Tile = {.Zoom = 2, .X = 1, .Y = 1},
                            .Nodes = {100, 101, 102, 103},
                            .Side = 2,
                            .Postings = 2}};
  region.Terrain.Tiles = 1;
  const std::string key(64, 'a');
  const std::string request(64, 'b');
  const auto saved = StoreGroundRegion(
      regions, key, GroundRegionBoundsEcef(region), region, Generators::WaterAsset{}, request);
  CHECK(saved, "the actual region package and request binding persist");
  if (!saved) { return Report(); }
  const auto before = regions.Costs();
  auto native = regions.LoadRequestProduct(request);
  CHECK(native && *native && (**native).Record().Key == key &&
            regions.Costs().Decodes == before.Decodes &&
            regions.Costs().ReadBytes > before.ReadBytes,
        "a stable request returns native bytes without allocating decoded world products");
  if (!native || !*native) { return Report(); }
  const auto decoded = regions.Decode(std::move(**native));
  CHECK(decoded && decoded->Key == key && decoded->Region.Terrain.Sheets == region.Terrain.Sheets &&
            regions.Costs().Decodes == before.Decodes + 1,
        "explicit consuming decode restores exact terrain and content identity once");
  const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
  const auto publish = [&](char id, char address, std::string kind) {
    const std::array<AssetRecord, 1> records{{{.Key = std::string(64, id),
                                               .Kind = std::move(kind),
                                               .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                               .Package = {},
                                               .ByteCount = bytes.size(),
                                               .Parent = {},
                                               .RequestKey = std::string(64, address)}}};
    return store->Publish(records, bytes);
  };
  CHECK(publish('c', 'd', "other-product") && publish('e', 'f', "ground-region"),
        "foreign-kind and damaged native packages are actual persisted controls");
  const auto counts = regions.Costs();
  const auto foreign = regions.LoadRequestProduct(std::string(64, 'd'));
  const auto missing = regions.LoadProduct(std::string(64, '0'));
  CHECK(foreign && !*foreign && missing && !*missing && regions.Costs().Decodes == counts.Decodes,
        "foreign kinds and genuine misses never invoke the domain decoder");
  auto direct = store->Load(std::string(64, 'c'), kLimit);
  CHECK(direct && *direct, "the generic store still supplies the unrelated product");
  if (direct && *direct) {
    CHECK(!regions.Decode(std::move(**direct)) && regions.Costs().Decodes == counts.Decodes,
          "the explicit decoder also checks its product boundary");
  }
  auto damaged = regions.LoadRequestProduct(std::string(64, 'f'));
  CHECK(damaged && *damaged && regions.Costs().Decodes == counts.Decodes,
        "native handoff does not conceal an implicit decode of damaged data");
  if (damaged && *damaged) {
    CHECK(!regions.Decode(std::move(**damaged)) && regions.Costs().Decodes == counts.Decodes + 1,
          "damaged region bytes fail only at the explicit domain decoder");
  }
  CHECK(raw.Counters().ReadCalls == 0 && inputs.Counters().ProviderStarts == 0,
        "native product reads and decode never consult source providers");
  std::filesystem::remove_all(directory);
  Covers("native cache reads, deferred consuming decode, source-free request identity, missing and "
         "damaged products; not global memory admission");
  return Report();
}
