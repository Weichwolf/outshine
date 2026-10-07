#include "content/AssetCache.h"
#include "Check.h"
#include "ContentStore.h"
#include "PreparedTerrainAssets.h"
#include "SourceSet.h"
#include "TerrainDeformationParts.h"
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto path = std::filesystem::temp_directory_path() /
                    ("outshine-terrain-parts-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto directory = (path / "assets").string();
  Data::ContentStore store({.Directory = (path / "sources").string()});
  Data::SourceSet sources(store);
  PreparedTerrainDeformation product;
  for (uint32_t index = 0; index < 18; ++index) {
    product.Pages.push_back({.Tile = {.Zoom = 20, .X = (1u << 19u) + index, .Y = 1u << 19u},
                             .Nodes = std::vector<float>(1024 * 1024, static_cast<float>(index)),
                             .Side = 1024,
                             .Postings = 1024,
                             .Virtual = false,
                             .SourceZoom = 20});
  }
  product.Effects.Nodes = 42;
  const std::string key(64, '7');
  const size_t bytesMost = TerrainDeformationHeap(product.Pages);
  CHECK(bytesMost > kTerrainDeformationBytesMost, "fixture exceeds a single bounded package");
  {
    auto cache = PreparedTerrainAssets::Open(directory, sources);
    CHECK(cache && (*cache)->StoreDeformation(key, product),
          "large region publishes all bounded parts");
    if (!cache) { return Report(); }
    CHECK(!(*cache)->LoadDeformation(key), "undersized caller residency budget rejects the load");
    CHECK((*cache)->Costs().DeformationWrites == 1, "a logical region is published once");
  }
  {
    auto cache = PreparedTerrainAssets::Open(directory, sources);
    CHECK(cache.has_value(), "independent restart opens native region");
    if (!cache) { return Report(); }
    auto loaded = (*cache)->LoadDeformation(key, bytesMost);
    CHECK(loaded && *loaded && (**loaded).Pages == product.Pages && (**loaded).Effects.Nodes == 42,
          "bounded parts replay every height and physical effect without source work");
    CHECK((*cache)->Costs().DeformationHits == 1 && (*cache)->Costs().DeformationWrites == 0,
          "restart consumes a native region hit");
  }
  auto storage = AssetCache::Open((path / "assets" / "assets.sqlite").string());
  CHECK(storage.has_value(), "native storage can inspect part bounds");
  if (!storage) { return Report(); }
  const auto partKey = TerrainDeformationPartKey(key, 0);
  auto record = (*storage)->Find(partKey);
  CHECK(record && *record && (**record).ByteCount <= kTerrainDeformationBytesMost &&
            (**record).Parent == key,
        "part has a bounded byte range and an explicit parent");
  CHECK((*storage)->Remove(partKey).has_value(), "simulate one missing package");
  auto cache = PreparedTerrainAssets::Open(directory, sources);
  CHECK(cache.has_value(), "region restarts after a part disappears");
  if (!cache) { return Report(); }
  auto partial = (*cache)->LoadDeformation(key, bytesMost);
  CHECK(partial && !*partial, "incomplete region becomes a miss instead of partial terrain");
  CHECK((*cache)->StoreDeformation(key, product).has_value(),
        "miss republishes the complete region");
  auto repaired = (*cache)->LoadDeformation(key, bytesMost);
  CHECK(repaired && *repaired && (**repaired).Pages == product.Pages,
        "repaired region is complete");
  std::filesystem::remove_all(path);
  return Report();
}
