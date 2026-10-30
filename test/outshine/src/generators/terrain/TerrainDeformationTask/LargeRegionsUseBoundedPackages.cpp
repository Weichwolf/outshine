#include "PublishTerrainDeformation.h"
#include <cstdint>
#include <cstddef>
#include "AssetCache.h"
#include "Check.h"
#include "RawSourceCache.h"
#include "PreparedTerrainAssets.h"
#include "SourceSet.h"
#include "TerrainDeformationParts.h"
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>
#include <variant>

namespace {
void PublishWithContention(outshine::Generators::PreparedTerrainAssets &cache,
                           const std::filesystem::path &directory,
                           const std::string &key,
                           const outshine::Generators::PreparedTerrainDeformation &product) {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto pending = PreparedTerrainAssets::PrepareDeformation(key, product);
  CHECK(pending.has_value(), "large native pages transfer to one resumable publication owner");
  if (!pending) { return; }
  const auto first = cache.StoreDeformation(*pending);
  CHECK(first && !*first, "one bounded native part publishes before the region root is available");
  auto records = AssetCache::Open((directory / "assets.sqlite").string());
  CHECK(records.has_value(), "native storage inspects publication visibility between parts");
  if (!records) { return; }
  const auto partKey = TerrainDeformationPartKey(key, 0);
  const auto publishedPart = (*records)->Find(partKey);
  const auto missingRoot = (*records)->Find(key);
  CHECK(publishedPart && *publishedPart && missingRoot && !*missingRoot,
        "a published part does not expose an incomplete native region");
  sqlite3 *connection = nullptr;
  CHECK(sqlite3_open((directory / "assets.sqlite").c_str(), &connection) == SQLITE_OK,
        "an independent connection controls inter-part writer admission");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(connection, sqlite3_close);
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "a different writer interrupts the second part after the first already succeeded");
  for (int attempt = 0; attempt < 3; ++attempt) {
    const auto blocked = cache.StoreDeformation(*pending);
    const auto *kind = blocked ? nullptr : std::get_if<AssetCacheError>(&blocked.error());
    CHECK(kind != nullptr && *kind == AssetCacheError::Busy,
          "inter-part writer contention remains a typed temporary error");
  }
  CHECK(cache.Costs().DeformationPreparations == 2 && cache.Costs().DeformationWrites == 0,
        "retries retain the second package and never reencode the completed first part");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer admits the remaining native parts");
  for (;;) {
    const auto stored = cache.StoreDeformation(*pending);
    CHECK(stored.has_value(), "the pending native package resumes without restarting prior parts");
    if (!stored || *stored) { break; }
  }
  CHECK(cache.StoreDeformation(*pending).value_or(false),
        "polling a completed native write is idempotent");
  const auto finalPart = (*records)->Find(partKey);
  CHECK(finalPart && *finalPart && publishedPart && *publishedPart &&
            **finalPart == **publishedPart,
        "interrupted continuation preserves the completed part's exact native record");
  CHECK(cache.Costs().DeformationPreparations == 3 && cache.Costs().DeformationWrites == 1,
        "two bounded page packages and the final root are encoded exactly once");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto path = std::filesystem::temp_directory_path() /
                    ("outshine-terrain-parts-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto directory = (path / "assets").string();
  Data::RawSourceCache store({.Directory = (path / "sources").string()});
  const Data::SourceSet sources(store);
  PreparedTerrainDeformation product;
  for (uint32_t index = 0; index < 18; ++index) {
    product.Pages.push_back(
        {.Tile = {.Zoom = 20, .X = (1u << 19u) + index, .Y = 1u << 19u},
         .Nodes = std::vector<float>(size_t{1024} * 1024, static_cast<float>(index)),
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
    CHECK(cache.has_value(), "the large native region cache opens");
    if (!cache) { return Report(); }
    PublishWithContention(**cache, directory, key, product);
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
  CHECK(PublishTerrainDeformation(**cache, key, product).has_value(),
        "miss republishes the complete region");
  auto repaired = (*cache)->LoadDeformation(key, bytesMost);
  CHECK(repaired && *repaired && (**repaired).Pages == product.Pages,
        "repaired region is complete");
  std::filesystem::remove_all(path);
  return Report();
}
