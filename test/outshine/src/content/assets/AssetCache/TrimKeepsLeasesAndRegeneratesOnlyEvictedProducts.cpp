#include "content/AssetGeneration.h"
#include "CacheTrim.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr std::array<uint8_t, 4> kOld{1, 3, 5, 7}, kNew{2, 4, 6, 8};

AssetRecord Record(char identity) {
  return {.Key = std::string(64, identity),
          .Kind = "trim-fixture",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .ByteCount = kOld.size()};
}

void Exercise(const std::filesystem::path &path) {
  auto opened = AssetCache::Open(path.string());
  CHECK(opened, "real native cache opens");
  if (!opened) { return; }
  std::optional<CachedAsset> held;
  const std::array oldRecords{Record('a')}, newRecords{Record('b')};
  CHECK((*opened)->Publish(oldRecords, kOld), "first immutable package publishes");
  CHECK((*opened)->Publish(newRecords, kNew), "second immutable package publishes");
  auto loaded = (*opened)->Load(oldRecords[0].Key, 1024);
  CHECK(loaded && *loaded, "a live lease holds the first package");
  if (!loaded || !*loaded) { return; }
  held = std::move(*loaded);
  opened->reset();
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.string().c_str(), &database) == SQLITE_OK,
        "independent deterministic recency fixture opens");
  CHECK(sqlite3_exec(database,
                     "UPDATE cache_usage_v1 SET created_unix_s=0,last_used_unix_s="
                     "(SELECT min(id) FROM assets WHERE assets.package=cache_usage_v1.key)",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "independent recency removes wall-clock boundary dependence");
  CHECK(sqlite3_close(database) == SQLITE_OK, "recency fixture closes");
  const std::array paths{path};
  auto plan = TrimAssetCaches(paths, {.BudgetBytes = kNew.size()});
  CHECK(plan && plan->RetainedBytes == kNew.size() && plan->PackagesRemoved == 1,
        "whole-package preview has an explicit byte budget");
  auto applied =
      TrimAssetCaches(paths, {.BudgetBytes = kNew.size(), .Apply = true, .Compact = true});
  CHECK(applied && applied->RetainedBytes == kNew.size(), "native package eviction completes");
  CHECK(std::ranges::equal(held->Bytes(), kOld),
        "a lease keeps immutable bytes alive after deletion and compaction");
  opened = AssetCache::Open(path.string());
  CHECK(opened, "native cache reopens after maintenance");
  if (!opened) { return; }
  auto old = (*opened)->Find(oldRecords[0].Key);
  auto recent = (*opened)->Load(newRecords[0].Key, 1024);
  CHECK(old && !*old, "evicted demand is a normal cache miss");
  CHECK(recent && *recent && std::ranges::equal((**recent).Bytes(), kNew),
        "retained demand still loads its complete original bytes");
  size_t generated = 0;
  auto restored = ResolveAsset(**opened,
                               oldRecords[0].Key,
                               1024,
                               [&](size_t) -> std::expected<GeneratedAssetPackage, std::string> {
                                 ++generated;
                                 return GeneratedAssetPackage{
                                     .Records = {oldRecords[0]},
                                     .Bytes = std::vector<uint8_t>(kOld.begin(), kOld.end())};
                               });
  CHECK(restored && generated == 1 && std::ranges::equal(restored->Bytes(), kOld),
        "one evicted demand regenerates through the ordinary miss path");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-cache-trim-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  Exercise(directory / "assets.sqlite");
  std::filesystem::remove_all(directory);
  return Report();
}
