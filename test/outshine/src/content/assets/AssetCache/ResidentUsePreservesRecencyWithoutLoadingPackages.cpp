#include "AssetCache.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

void Exercise(const std::filesystem::path &path) {
  auto cache = AssetCache::Open(path.string());
  CHECK(cache.has_value(), "resident-use cache opens");
  if (!cache) { return; }
  const std::vector<uint8_t> bytes(4096, 42);
  const std::array records{AssetRecord{.Key = std::string(64, 'a'),
                                       .Kind = "resident-use",
                                       .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                       .Package = {},
                                       .OffsetBytes = 0,
                                       .ByteCount = bytes.size(),
                                       .Level = 0,
                                       .Parent = {},
                                       .RequestKey = {}}};
  CHECK((*cache)->Publish(records, bytes).has_value(), "compressible fixture publishes");
  auto loaded = (*cache)->Load(records.front().Key, 8192);
  CHECK(loaded && *loaded, "the initial consumer owns a verified immutable lease");
  if (!loaded || !*loaded) { return; }
  const auto package = (**loaded).Record().Package;
  cache->reset();
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open(path.string().c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  CHECK(opened == SQLITE_OK, "independent usage fixture opens");
  if (opened != SQLITE_OK) { return; }
  CHECK(sqlite3_exec(database.get(),
                     "UPDATE cache_usage_v1 SET stored_bytes=1,created_unix_s=NULL,"
                     "last_used_unix_s=1;UPDATE packages SET crc=crc+1;",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "old unknown creation and unreadable disk payload are deterministic fixtures");
  cache = AssetCache::Open(path.string());
  CHECK(cache.has_value(), "a fresh metadata observer has no pending initial load");
  if (!cache) { return; }
  (*cache)->UsePackage(package);
  cache->reset();
  sqlite3_stmt *statementRaw = nullptr;
  const int prepared = sqlite3_prepare_v2(
      database.get(),
      "SELECT u.stored_bytes,length(b.bytes),u.created_unix_s,u.last_used_unix_s "
      "FROM cache_usage_v1 u JOIN packages p ON p.key=u.key "
      "JOIN package_bodies b ON b.key=p.key",
      -1,
      &statementRaw,
      nullptr);
  std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(statementRaw,
                                                                       &sqlite3_finalize);
  CHECK(prepared == SQLITE_OK && sqlite3_step(statement.get()) == SQLITE_ROW,
        "resident-only usage persists through the ordinary batched index");
  if (prepared != SQLITE_OK || sqlite3_data_count(statement.get()) != 4) { return; }
  const auto stored = sqlite3_column_int64(statement.get(), 0);
  CHECK(stored > 0 && std::cmp_less(stored, bytes.size()) &&
            stored == sqlite3_column_int64(statement.get(), 1),
        "resident use records actual compressed package bytes rather than decoded slice size");
  CHECK(sqlite3_column_type(statement.get(), 2) == SQLITE_NULL &&
            sqlite3_column_int64(statement.get(), 3) > 1,
        "resident use refreshes recency without inventing an old creation time");
  CHECK(std::ranges::equal((**loaded).Bytes(), bytes),
        "the original consumer remains valid independently of changed disk integrity");
  statement.reset();
  cache = AssetCache::Open(path.string());
  CHECK(cache.has_value(), "disk integrity control opens");
  if (!cache) { return; }
  const auto invalid = (*cache)->Load(records.front().Key, 8192);
  CHECK(invalid && !*invalid,
        "the invalid disk CRC proves recency refresh did not depend on a payload load");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-resident-use-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  Exercise(directory / "assets.sqlite");
  std::filesystem::remove_all(directory);
  return Report();
}
