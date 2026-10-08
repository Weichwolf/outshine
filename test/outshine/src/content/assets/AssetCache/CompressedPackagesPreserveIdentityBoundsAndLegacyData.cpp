#include "content/AssetCache.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <sqlite3.h>
#include <zlib.h>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::string Key(std::string_view value) {
  return Sha256Hex(value.data(), value.size());
}

AssetRecord Record(std::string_view name, size_t bytes, size_t offset = 0) {
  return {.Key = Key(name),
          .Kind = "native",
          .Bounds = {.Min = {}, .Max = {{1, 1, 1}}},
          .OffsetBytes = offset,
          .ByteCount = bytes};
}

bool Execute(const std::string &path, const char *sql) {
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open(path.c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  return opened == SQLITE_OK &&
         sqlite3_exec(database.get(), sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void ReadsCompressedSlices(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache.has_value(), "cache opens for compressed native assets");
  if (!cache) { return; }
  std::vector<uint8_t> bytes(1024 * 1024);
  for (size_t index = 0; index < bytes.size(); index += 1024) { bytes[index] = 91; }
  const std::array records{Record("first", 4096), Record("second", 3072, bytes.size() - 3072)};
  CHECK((*cache)->Publish(records, bytes).has_value(), "compressible package publishes atomically");
  cache->reset();
  cache = AssetCache::Open(path);
  if (!cache) { return; }
  for (const auto &record : records) {
    const auto hit = (*cache)->Load(record.Key, bytes.size());
    CHECK(hit && *hit && (**hit).Record().Package == Sha256Hex(bytes.data(), bytes.size()) &&
              std::ranges::equal((**hit).Bytes(),
                                 std::span(bytes).subspan(record.OffsetBytes, record.ByteCount)),
          "restart decodes exact slices while package identity remains native-byte based");
  }
  const auto refused = (*cache)->Load(records.front().Key, bytes.size() - 1);
  CHECK(!refused && refused.error() == AssetCacheError::CapacityExceeded,
        "compressed packages retain the whole decoded-package allocation bound");
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open(path.c_str(), &raw) == SQLITE_OK,
        "inspect storage without altering product keys");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  sqlite3_stmt *statement = nullptr;
  CHECK(sqlite3_prepare_v2(database.get(),
                           "SELECT codec,length(bytes),native_bytes FROM packages",
                           -1,
                           &statement,
                           nullptr) == SQLITE_OK,
        "explicit codec and decoded size are persisted");
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> query(statement,
                                                                         &sqlite3_finalize);
  CHECK(sqlite3_step(query.get()) == SQLITE_ROW && sqlite3_column_int(query.get(), 0) == 1 &&
            sqlite3_column_int64(query.get(), 1) < 4096 &&
            sqlite3_column_int64(query.get(), 2) == static_cast<sqlite3_int64>(bytes.size()),
        "sparse megabyte product uses compact storage without cutting detail");
}

void KeepsLegacyBytes(const std::string &path) {
  const std::array<uint8_t, 8> bytes{1, 3, 5, 7, 9, 11, 13, 15};
  const auto record = Record("legacy", bytes.size());
  auto cache = AssetCache::Open(path);
  if (!cache) { return; }
  CHECK((*cache)->Publish(std::span(&record, 1), bytes).has_value(), "small package remains raw");
  cache->reset();
  CHECK(Execute(path,
                "ALTER TABLE packages DROP COLUMN codec; ALTER TABLE packages DROP COLUMN "
                "native_bytes; DROP INDEX asset_requests; ALTER TABLE assets DROP COLUMN "
                "request_key; PRAGMA user_version=1;"),
        "fixture has the original schema and untouched legacy package bytes");
  cache = AssetCache::Open(path);
  CHECK(cache.has_value(), "schema migration opens legacy packages without regeneration");
  if (!cache) { return; }
  const auto hit = (*cache)->Load(record.Key, bytes.size());
  CHECK(hit && *hit && std::ranges::equal((**hit).Bytes(), bytes),
        "legacy raw payload and identity survive migration");
  CHECK((*cache)->Publish(std::span(&record, 1), bytes).has_value(),
        "migrated cache still publishes native assets");
}

void RefusesDamagedPackages(const std::string &path) {
  std::vector<uint8_t> bytes(32768);
  const auto record = Record("damage", bytes.size());
  auto cache = AssetCache::Open(path);
  if (!cache) { return; }
  const auto restore = [&] { return (*cache)->Publish(std::span(&record, 1), bytes).has_value(); };
  CHECK(restore(), "valid compressed fixture publishes");
  CHECK(Execute(path, "UPDATE packages SET codec=3"), "fixture injects unknown codec");
  auto hit = (*cache)->Load(record.Key, bytes.size());
  CHECK(hit && !*hit, "unknown codec is a miss, never interpreted as raw bytes");
  CHECK(restore() && Execute(path, "UPDATE packages SET native_bytes=1073741824"),
        "fixture injects excessive decoded size");
  hit = (*cache)->Load(record.Key, bytes.size());
  CHECK(!hit && hit.error() == AssetCacheError::CapacityExceeded,
        "decoded size is bounded before package allocation");
  CHECK(restore() && Execute(path, "UPDATE packages SET native_bytes=32769"),
        "fixture injects inconsistent frame size");
  hit = (*cache)->Load(record.Key, bytes.size() + 1);
  CHECK(hit && !*hit, "frame size must agree with metadata before decompression");
  CHECK(restore() && Execute(path, "UPDATE packages SET bytes=x'00',crc=3523407757"),
        "fixture injects CRC-valid invalid frame");
  hit = (*cache)->Load(record.Key, bytes.size());
  CHECK(hit && !*hit, "valid storage CRC cannot make an invalid compressed frame usable");
  CHECK(restore() && Execute(path, "UPDATE packages SET crc=0"),
        "fixture damages storage checksum");
  hit = (*cache)->Load(record.Key, bytes.size());
  CHECK(hit && !*hit, "physical payload corruption remains an ordinary miss");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-native-compression-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  ReadsCompressedSlices((root / "compressed.sqlite").string());
  KeepsLegacyBytes((root / "legacy.sqlite").string());
  RefusesDamagedPackages((root / "damage.sqlite").string());
  std::filesystem::remove_all(root);
  return Report();
}
