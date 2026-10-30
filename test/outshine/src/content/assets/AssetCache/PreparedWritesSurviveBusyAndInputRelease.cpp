#include "AssetCache.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <sqlite3.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

AssetRecord Record(char key, size_t bytes) {
  return {.Key = std::string(64, key),
          .Kind = "prepared-write",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = {},
          .ByteCount = bytes,
          .Parent = {},
          .RequestKey = std::string(64, 'b')};
}

void OwnedPackageSurvivesContention(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened, "the native publication store opens");
  if (!opened) { return; }
  auto &cache = **opened;
  const std::vector<uint8_t> expected(size_t{64} * 1024, 23);
  auto input = expected;
  std::vector records{Record('a', input.size())};
  sqlite3 *writer = nullptr;
  CHECK(sqlite3_open(path.c_str(), &writer) == SQLITE_OK,
        "an independent SQLite writer opens the same store");
  if (writer == nullptr) { return; }
  const auto locked = sqlite3_exec(writer, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
  CHECK(locked == SQLITE_OK, "the independent writer holds publication admission");
  if (locked != SQLITE_OK) {
    sqlite3_close(writer);
    return;
  }
  auto prepared = cache.PreparePublication(std::move(records), std::move(input));
  CHECK(prepared, "validation and compression finish without acquiring a writer transaction");
  records.assign(1, Record('c', 1));
  input.assign(1, 0);
  if (prepared) {
    CHECK(prepared->NativeBytes() == expected.size() &&
              prepared->StoredBytes() < prepared->NativeBytes(),
          "the prepared owner retains the encoded package after the caller releases its inputs");
    const auto bytes = prepared->StoredBytes();
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
      const auto blocked = cache.Publish(*prepared);
      CHECK(!blocked && blocked.error() == AssetCacheError::Busy &&
                prepared->StoredBytes() == bytes,
            "each rejected admission leaves the same complete encoded package available");
    }
    const auto absent = cache.FindRequest(std::string(64, 'b'));
    CHECK(absent && !*absent, "busy publication exposes no partial request binding");
  }
  CHECK(sqlite3_exec(writer, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the independent writer releases publication admission");
  sqlite3_close(writer);
  if (!prepared) { return; }
  auto retained = std::move(*prepared);
  CHECK(cache.Publish(retained), "the retained package publishes after contention ends");
  auto fresh = AssetCache::Open(path);
  CHECK(fresh, "a fresh native owner opens the completed publication");
  if (!fresh) { return; }
  const auto loaded = (*fresh)->LoadRequest(std::string(64, 'b'), expected.size());
  CHECK(loaded && *loaded && (**loaded).Record().Key == std::string(64, 'a') &&
            std::ranges::equal((**loaded).Bytes(), expected),
        "fresh lookup restores the original bytes and metadata after all caller inputs changed");
}

void UncompressedWritesAndInvalidInputs(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened, "the uncompressed publication store opens");
  if (!opened) { return; }
  auto &cache = **opened;
  const std::vector<uint8_t> expected{1, 2, 3, 4, 5, 6, 7, 8};
  auto record = Record('c', expected.size());
  record.RequestKey.reset();
  auto prepared = cache.PreparePublication({record}, expected);
  CHECK(prepared && prepared->StoredBytes() == expected.size() && cache.Publish(*prepared),
        "an incompressible package owns and publishes the original bytes");
  const auto loaded = cache.Load(record.Key, expected.size());
  CHECK(loaded && *loaded && std::ranges::equal((**loaded).Bytes(), expected),
        "the uncompressed prepared package preserves every original byte");
  const auto missing = cache.PreparePublication({}, expected);
  CHECK(!missing && missing.error() == AssetCacheError::InvalidInput,
        "a prepared write requires at least one valid asset record");
  ++record.ByteCount;
  const auto invalid = cache.PreparePublication({record}, expected);
  CHECK(!invalid && invalid.error() == AssetCacheError::InvalidInput,
        "a prepared record cannot name bytes outside its owned package");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(!std::is_copy_constructible_v<PreparedAssetWrite>, "prepared writes cannot copy ownership");
  CHECK(!std::is_copy_assignable_v<PreparedAssetWrite>,
        "prepared write assignment cannot copy ownership");
  CHECK(std::is_nothrow_move_constructible_v<PreparedAssetWrite>,
        "prepared writes move without throwing");
  CHECK(std::is_nothrow_move_assignable_v<PreparedAssetWrite>,
        "prepared write assignment moves without throwing");
  const std::filesystem::path root(PlantedPath("prepared-asset-write"));
  std::filesystem::create_directories(root);
  OwnedPackageSurvivesContention((root / "compressed.sqlite").string());
  UncompressedWritesAndInvalidInputs((root / "raw.sqlite").string());
  std::filesystem::remove_all(root);
  return Report();
}
