#include "Check.h"
#include "content/AssetGeneration.h"
#include "Sha256.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::string Key(std::string_view value) {
  return Sha256Hex(value.data(), value.size());
}

GeneratedAssetPackage Package(std::string_view revision, std::string request) {
  GeneratedAssetPackage result;
  result.Bytes.assign(revision.begin(), revision.end());
  result.Records.push_back({.Key = Key(revision),
                            .Kind = "native-world-cell",
                            .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1000, 1000, 1000}}},
                            .ByteCount = result.Bytes.size(),
                            .RequestKey = std::nullopt});
  if (!request.empty()) { result.Records.back().RequestKey = std::move(request); }
  return result;
}

void FreshHits(const std::string &path) {
  const auto request = Key("generator-v1/config/tile-14-1024-2048/lod-2/seed-7");
  size_t sources = 0;
  auto cache = AssetCache::Open(path);
  CHECK(cache.has_value(), "request index opens");
  if (!cache) { return; }
  auto cold = ResolveAssetRequest(**cache, request, 64, [&](size_t) {
    ++sources;
    return std::expected<GeneratedAssetPackage, std::string>(Package("input-one", request));
  });
  CHECK(cold && sources == 1 && cold->Record().Key == Key("input-one"),
        "only the miss discovers the content identity through its inputs");
  cache->reset();
  cache = AssetCache::Open(path);
  CHECK(cache.has_value(), "fresh cache instance reopens the request binding");
  if (!cache) { return; }
  auto hit = ResolveAssetRequest(**cache, request, 64, {});
  CHECK(hit && hit->Record().RequestKey == request && hit->Bytes().size() == 9 && sources == 1,
        "offline hit resolves from known demand without any source or generator");
  auto changed = Package("input-two", request);
  CHECK((*cache)->Publish(changed.Records, changed.Bytes).has_value(),
        "explicit input change publishes the new binding with its complete product");
  hit = ResolveAssetRequest(**cache, request, 64, {});
  CHECK(hit && hit->Record().Key == Key("input-two") && hit->Bytes().back() == 'o' && cold &&
            cold->Bytes().back() == 'e',
        "new requests use the new product while the old lease retains its own bytes");
  const auto previous = (*cache)->Find(Key("input-one"));
  CHECK(previous && *previous && !(**previous).RequestKey,
        "previous content remains addressable without retaining the current request binding");
  auto duplicate = Package("ambiguous", request);
  auto other = duplicate.Records.front();
  other.Key = Key("other-ambiguous");
  duplicate.Records.push_back(other);
  const auto refused = (*cache)->Publish(duplicate.Records, duplicate.Bytes);
  const auto current = (*cache)->FindRequest(request);
  CHECK(!refused && refused.error() == AssetCacheError::InvalidInput && current && *current &&
            (**current).Key == Key("input-two") && !(*cache)->Find(other.Key)->has_value(),
        "ambiguous demand cannot change either the existing binding or package index");
  CHECK(!(*cache)->FindRequest("bad") && !(*cache)->FindRequest(""),
        "request identities validate before metadata lookup");
  CHECK((*cache)->Remove(Key("input-two")).has_value() &&
            !(*cache)->FindRequest(request)->has_value(),
        "removing a product removes its demand binding without leaving a false hit");
  const auto incomplete = ResolveAssetRequest(**cache, request, 64, [](size_t) {
    return std::expected<GeneratedAssetPackage, std::string>(Package("unrelated", {}));
  });
  CHECK(!incomplete && !(*cache)->Find(Key("unrelated"))->has_value(),
        "factory that omits the demand publishes nothing");
}

void MigratesVersionTwo(const std::string &path) {
  auto cache = AssetCache::Open(path);
  if (!cache) { return; }
  auto original = Package("legacy-two", {});
  CHECK((*cache)->Publish(original.Records, original.Bytes).has_value(),
        "legacy fixture publishes");
  cache->reset();
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "migration fixture opens");
  if (!database) { return; }
  CHECK(sqlite3_exec(database,
                     "DROP INDEX asset_requests; ALTER TABLE assets DROP COLUMN request_key;"
                     "PRAGMA user_version=2;",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "fixture retains version-two products without demand metadata");
  sqlite3_close(database);
  cache = AssetCache::Open(path);
  CHECK(cache.has_value(), "migration preserves the existing cache instead of discarding it");
  if (!cache) { return; }
  const auto loaded = (*cache)->Load(original.Records.front().Key, 64);
  CHECK(loaded && *loaded && (**loaded).Bytes().size() == original.Bytes.size() &&
            !(**loaded).Record().RequestKey,
        "native payload, content identity and ordinary lookup survive version-two migration");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-native-requests-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  FreshHits((root / "requests.sqlite").string());
  MigratesVersionTwo((root / "legacy.sqlite").string());
  std::filesystem::remove_all(root);
  return Report();
}
