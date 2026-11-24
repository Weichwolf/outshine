#include "AssetCache.h"
#include "Check.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <sqlite3.h>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckBatch(const std::string &path) {
  const auto cache = AssetCache::Open(path);
  CHECK(cache, "the batch store opens");
  if (!cache) { return; }
  const std::vector<uint8_t> firstBytes(2048, 11);
  const std::vector<uint8_t> secondBytes(4096, 3);
  const AssetRecord first{.Key = std::string(64, 'a'),
                          .Kind = "first",
                          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                          .Package = {},
                          .ByteCount = firstBytes.size(),
                          .Parent = {}};
  auto second = first;
  second.Key = std::string(64, 'b');
  second.Kind = "second";
  second.ByteCount = secondBytes.size();
  auto a = (*cache)->PreparePublication({first}, firstBytes);
  auto b = (*cache)->PreparePublication({second}, secondBytes);
  CHECK(a && b, "two independently compressed packages are prepared");
  if (!a || !b) { return; }
  std::vector<PreparedAssetWrite> batch;
  batch.push_back(std::move(*a));
  batch.push_back(std::move(*b));
  sqlite3 *writer = nullptr;
  if (sqlite3_open(path.c_str(), &writer) != SQLITE_OK) {
    if (writer != nullptr) { sqlite3_close(writer); }
    CHECK(false, "the fixture can add its late failure");
    return;
  }
  const char *trigger =
      "CREATE TRIGGER refuse_second BEFORE INSERT ON assets WHEN NEW.kind='second' "
      "BEGIN SELECT RAISE(ABORT, 'test'); END";
  const int armed = sqlite3_exec(writer, trigger, nullptr, nullptr, nullptr);
  CHECK(armed == SQLITE_OK, "the last package is forced to fail");
  if (armed == SQLITE_OK) {
    CHECK(!(*cache)->PublishBatch(batch), "the rejected second package fails the batch");
    const auto absent = (*cache)->Find(first.Key);
    CHECK(absent && !*absent, "the first asset is also rolled back");
  }
  const int unlocked =
      sqlite3_exec(writer, "DROP TRIGGER IF EXISTS refuse_second", nullptr, nullptr, nullptr);
  sqlite3_close(writer);
  if (unlocked != SQLITE_OK) {
    CHECK(false, "the fixture can retry");
    return;
  }
  CHECK((*cache)->PublishBatch(batch), "the same owned bytes retry successfully");
  const auto loaded = (*cache)->Load(second.Key, secondBytes.size());
  CHECK(loaded && *loaded && std::ranges::equal((**loaded).Bytes(), secondBytes),
        "each member retains an independently readable payload");
}
}

int main() {
  using namespace outshine::Test;
  const std::filesystem::path root(PlantedPath("prepared-asset-batch"));
  std::filesystem::create_directories(root);
  CheckBatch((root / "assets.sqlite").string());
  std::filesystem::remove_all(root);
  return Report();
}
