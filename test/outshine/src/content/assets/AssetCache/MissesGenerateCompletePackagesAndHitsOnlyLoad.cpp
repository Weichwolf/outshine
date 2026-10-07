#include "generation/AssetGeneration.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <span>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

const std::array<uint8_t, 12> kPayload{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

std::string Key(std::string_view name) {
  return Sha256Hex(name.data(), name.size());
}

GeneratedAssetPackage Package() {
  GeneratedAssetPackage package;
  package.Bytes.assign(kPayload.begin(), kPayload.end());
  for (const auto *kind : {"terrain", "roads", "buildings", "vegetation"}) {
    package.Records.push_back({.Key = Key(kind),
                               .Kind = kind,
                               .Bounds = {.Min = {{0, 0, 0}}, .Max = {{10, 10, 10}}},
                               .OffsetBytes = package.Records.size() * 3,
                               .ByteCount = 3});
  }
  return package;
}

void RefusesIncompleteWork(AssetCache &cache) {
  const auto refused = ResolveAsset(cache, Key("failed"), 16, [](size_t) {
    return std::expected<GeneratedAssetPackage, std::string>(std::unexpected("provider failed"));
  });
  CHECK(!refused && refused.error() == "provider failed",
        "provider refusal remains a real generation failure");
  CHECK(!cache.Load(Key("failed"), 16)->has_value(),
        "failed generation cannot publish a cache hit");
  const auto missing = ResolveAsset(cache, Key("missing"), 16, [](size_t) { return Package(); });
  CHECK(!missing && !cache.Find(Key("terrain"))->has_value(),
        "omitted demand cannot publish an unrelated valid package");
  auto invalid = Package();
  invalid.Records[2].ByteCount = std::numeric_limits<uint64_t>::max();
  const auto partial = cache.Publish(invalid.Records, invalid.Bytes);
  CHECK(!partial && !cache.Find(Key("terrain"))->has_value(),
        "invalid slices reject all bytes and metadata atomically");
}

void FreshHitsAndSnapshots(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened.has_value(), "native asset cache opens");
  if (!opened) { return; }
  RefusesIncompleteWork(**opened);
  size_t generated = 0;
  const AssetFactory factory =
      [&](size_t budget) -> std::expected<GeneratedAssetPackage, std::string> {
    CHECK(budget == 16, "generator receives the requested payload budget before doing work");
    ++generated;
    return Package();
  };
  auto cold = ResolveAsset(**opened, Key("terrain"), 16, factory);
  CHECK(cold && generated == 1 && cold->Bytes().size() == 3 && cold->Bytes().front() == 1,
        "one miss generates the complete package and loads its requested slice");
  opened->reset();
  auto restarted = AssetCache::Open(path);
  CHECK(restarted.has_value(), "a fresh cache instance opens stored assets");
  if (!restarted) { return; }
  const AssetFactory offline = [&](size_t) -> std::expected<GeneratedAssetPackage, std::string> {
    ++generated;
    return std::unexpected("provider unavailable offline");
  };
  for (const auto *kind : {"terrain", "roads", "buildings", "vegetation"}) {
    const auto hit = ResolveAsset(**restarted, Key(kind), 16, offline);
    CHECK(hit && generated == 1 && hit->Bytes().size() == 3,
          "all prepared content kinds load without any provider or generator work");
  }
  const auto constrained = (*restarted)->Load(Key("terrain"), 2);
  CHECK(!constrained && constrained.error() == AssetCacheError::CapacityExceeded,
        "whole-package budget is checked before allocating its payload");
  auto changed = Package();
  changed.Bytes.front() = 42;
  CHECK((*restarted)->Publish(changed.Records, changed.Bytes).has_value(),
        "replacement publishes all slices atomically");
  auto replacement = (*restarted)->Load(Key("terrain"), 16);
  CHECK(replacement && *replacement && (**replacement).Bytes().front() == 42 &&
            cold->Bytes().front() == 1,
        "new readers observe replacement while held bytes remain immutable");
  restarted->reset();
  CHECK(cold->Bytes().back() == 3, "loaded asset bytes outlive the cache connection");
}

void RepairsCorruption(const std::string &path) {
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK &&
            sqlite3_exec(database, "UPDATE packages SET crc=-1", nullptr, nullptr, nullptr) ==
                SQLITE_OK,
        "fixture corrupts package checksums without changing metadata");
  sqlite3_close(database);
  auto opened = AssetCache::Open(path);
  if (!opened) {
    CHECK(false, "cache must reopen corrupted payload fixture");
    return;
  }
  CHECK(!(*opened)->Load(Key("terrain"), 16)->has_value(),
        "corrupt bytes are misses rather than native assets");
  size_t generated = 0;
  const auto rebuilt = ResolveAsset(**opened, Key("terrain"), 16, [&](size_t) {
    ++generated;
    return std::expected<GeneratedAssetPackage, std::string>(Package());
  });
  CHECK(rebuilt && generated == 1 && rebuilt->Bytes().front() == 1,
        "corruption causes exactly one fresh generation and republishes a complete package");
  const AssetRecord empty{.Key = Key("empty"), .Kind = "water", .Bounds = {.Min = {}, .Max = {}}};
  CHECK((*opened)->Publish(std::span(&empty, 1), {}).has_value(),
        "known empty asset is persistently representable");
  const auto known = (*opened)->Load(empty.Key, 0);
  CHECK(known && *known && (**known).Bytes().empty(),
        "known empty asset remains distinguishable from a miss");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-asset-cache-packages-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = (root / "assets.sqlite").string();
  FreshHitsAndSnapshots(path);
  RepairsCorruption(path);
  std::filesystem::remove_all(root);
  return Report();
}
