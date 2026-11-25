#include "OpenAssetStore.h"
#include "StoreGroundRegion.h"
#include "PreparedGroundRegions.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Check.h"
#include "AssetCache.h"
#include "AssetRecord.h"
#include "GroundRegionAsset.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckTerrainPacks(AssetCache &store,
                       Generators::Osm::PreparedGroundRegions &regions,
                       GroundRegionAsset &region,
                       std::span<const Sheet> resident,
                       const std::string &key,
                       const std::string &request) {
  const auto index = [&](const std::string &revision) {
    return store.Select({.Bounds = GroundRegionBoundsEcef(region),
                         .Kind = "ground-terrain-pack",
                         .Level = {},
                         .Radius = {},
                         .Planes = {},
                         .Parent = revision,
                         .TileRevision = revision});
  };
  const auto packs = index(key);
  CHECK(packs && packs->size() == 2 && (*packs)[0].Package != (*packs)[1].Package,
        "different spatial groups can be found and read independently");
  if (packs && packs->size() == 2) {
    std::ranges::reverse(region.Terrain.Sheets);
    const std::string otherKey(64, '9');
    const auto other = StoreGroundRegion(
        regions, otherKey, GroundRegionBoundsEcef(region), region, Generators::WaterAsset{});
    const auto root = store.Find(otherKey);
    const auto shared = index(otherKey);
    CHECK(other && other->Region.Terrain.Sheets == region.Terrain.Sheets && shared &&
              shared->size() == packs->size() &&
              std::ranges::all_of(*packs,
                                  [&](const auto &pack) {
                                    return std::ranges::contains(
                                        *shared, pack.Package, &AssetRecord::Package);
                                  }),
          "reordered regions share the compressed terrain without changing draw order");
    CHECK(other && root && *root && other->ReadBytes == (**root).ByteCount &&
              other->Region.Terrain.Sheets.front().Samples().data() ==
                  resident.back().Samples().data(),
          "overlapping camera regions reuse the live heights and only read their new header");
    std::ranges::reverse(region.Terrain.Sheets);
    CHECK(store.Remove(packs->front().Key), "terrain pack can be evicted separately");
    auto partial = regions.LoadRequestProduct(request);
    CHECK(partial && *partial, "the region header survives independent pack eviction");
    if (partial && *partial) {
      const auto missingTerrain = regions.Restore(std::move(**partial));
      CHECK(missingTerrain && !*missingTerrain, "incomplete terrain does not publish holes");
      CHECK(StoreGroundRegion(regions,
                              key,
                              GroundRegionBoundsEcef(region),
                              region,
                              Generators::WaterAsset{},
                              request),
            "a miss can restore complete coverage under the same request");
    }
  }
}
}

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
  region.Terrain.Sheets = {{.Tile = {.Zoom = 5, .X = 18, .Y = 1},
                            .Nodes = {100, 101, 102, 103},
                            .Side = 2,
                            .Postings = 2},
                           {.Tile = {.Zoom = 5, .X = 1, .Y = 1},
                            .Nodes = {105, 106, 107, 108},
                            .Side = 2,
                            .Postings = 2},
                           {.Tile = {.Zoom = 5, .X = 19, .Y = 1},
                            .Nodes = {110, 111, 112, 113},
                            .Side = 2,
                            .Postings = 2}};
  region.Terrain.Tiles = region.Terrain.Sheets.size();
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
  const auto decoded = regions.Restore(std::move(**native));
  CHECK(decoded && *decoded && (**decoded).Key == key &&
            (**decoded).Region.Terrain.Sheets == region.Terrain.Sheets &&
            regions.Costs().Decodes == before.Decodes + 1,
        "independent terrain packs restore the original draw order and heights");
  CheckTerrainPacks(*store, regions, region, saved->Region.Terrain.Sheets, key, request);
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
    const auto restored = regions.Restore(std::move(**direct));
    CHECK(restored && !*restored && regions.Costs().Decodes == counts.Decodes,
          "the explicit decoder also checks its product boundary");
  }
  auto damaged = regions.LoadRequestProduct(std::string(64, 'f'));
  CHECK(damaged && *damaged && regions.Costs().Decodes == counts.Decodes,
        "native handoff does not conceal an implicit decode of damaged data");
  if (damaged && *damaged) {
    const auto restored = regions.Restore(std::move(**damaged));
    CHECK(restored && !*restored && regions.Costs().Decodes == counts.Decodes + 1,
          "damaged region bytes fail only at the explicit domain decoder");
  }
  CHECK(raw.Counters().ReadCalls == 0 && inputs.Counters().ProviderStarts == 0,
        "native product reads and decode never consult source providers");
  std::filesystem::remove_all(directory);
  Covers("native cache reads, deferred consuming decode, source-free request identity, missing and "
         "damaged products; not global memory admission");
  return Report();
}
