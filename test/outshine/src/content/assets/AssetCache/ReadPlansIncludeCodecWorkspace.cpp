#include "AssetCache.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <expected>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
unsigned blobOpens = 0;
unsigned blobReads = 0;

void CheckRead(const std::string &path) {
  auto store = AssetCache::Open(path);
  CHECK(store, "the read-plan fixture opens a real asset store");
  if (!store) { return; }
  const std::string key(64, 'a');
  std::vector<uint8_t> bytes(4096);
  uint32_t state = 0x9e3779b9;
  for (auto &byte : bytes) {
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    byte = static_cast<uint8_t>(state);
  }
  const std::array<AssetRecord, 1> records{{{.Key = key,
                                             .Kind = "planned-read",
                                             .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                             .Package = {},
                                             .OffsetBytes = 0,
                                             .ByteCount = bytes.size(),
                                             .Parent = {},
                                             .RequestKey = {}}}};
  CHECK((*store)->Publish(records, bytes), "an incompressible package publishes uncompressed");
  blobOpens = blobReads = 0;
  const auto raw = (*store)->PlanRead(key, bytes.size());
  CHECK(raw && *raw && (**raw).PackageBytesMost == bytes.size() &&
            (**raw).BufferBytesMost == bytes.size() && blobOpens == 0 && blobReads == 0,
        "raw metadata planning reserves one buffer without opening or reading the package blob");
  if (!raw || !*raw) { return; }
  std::ranges::fill(bytes, uint8_t{0});
  CHECK((*store)->Publish(records, bytes),
        "a same-size compressed winner replaces the raw product");
  blobOpens = blobReads = 0;
  const auto compressed = (*store)->PlanRead(key, bytes.size());
  CHECK(compressed && *compressed && (**compressed).PackageBytesMost == bytes.size() &&
            (**compressed).BufferBytesMost > 3 * bytes.size() && blobOpens == 0 && blobReads == 0,
        "compressed metadata includes the decoder context beyond the former three-buffer bound");
  if (!compressed || !*compressed) { return; }
  auto oneShort = **compressed;
  --oneShort.BufferBytesMost;
  const auto stale = (*store)->Load(key, **raw);
  const auto shortRead = (*store)->Load(key, oneShort);
  const auto oldBound = (*store)->Load(
      key,
      AssetCache::ReadPlan{.PackageBytesMost = bytes.size(), .BufferBytesMost = 3 * bytes.size()});
  CHECK(!stale && stale.error() == AssetCacheError::CapacityExceeded && !shortRead &&
            shortRead.error() == AssetCacheError::CapacityExceeded && !oldBound &&
            oldBound.error() == AssetCacheError::CapacityExceeded && blobOpens == 0 &&
            blobReads == 0,
        "changed codec, one-byte-short and obsolete reservations all refuse before blob IO");
  const auto loaded = (*store)->Load(key, **compressed);
  CHECK(loaded && *loaded && std::ranges::equal((**loaded).Bytes(), bytes) && blobOpens == 1 &&
            blobReads > 0,
        "the exact complete read plan loads the surviving winner without corrupting its data");
  blobOpens = blobReads = 0;
  const auto validated = (*store)->LoadValidated(
      key,
      "planned-read",
      oneShort,
      []([[maybe_unused]] const CachedAsset &asset) -> std::expected<bool, AssetCacheError> {
        return true;
      });
  CHECK(!validated && validated.error() == AssetCacheError::CapacityExceeded && blobOpens == 0 &&
            blobReads == 0 && (*store)->Find(key)->has_value(),
        "validated loading enforces the same complete bound and preserves an unread valid winner");
}
}

extern "C" int sqlite3_blob_open(sqlite3 *database,
                                 const char *zDb,
                                 const char *zTable,
                                 const char *zColumn,
                                 sqlite3_int64 iRow,
                                 int flags,
                                 sqlite3_blob **ppBlob) {
  using Open = int (*)(
      sqlite3 *, const char *, const char *, const char *, sqlite3_int64, int, sqlite3_blob **);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_blob_open"));
  if (open == nullptr) { return SQLITE_ERROR; }
  ++blobOpens;
  return open(database, zDb, zTable, zColumn, iRow, flags, ppBlob);
}

extern "C" int sqlite3_blob_read(sqlite3_blob *blob, void *Z, int N, int iOffset) {
  using Read = int (*)(sqlite3_blob *, void *, int, int);
  static const auto read = reinterpret_cast<Read>(dlsym(RTLD_NEXT, "sqlite3_blob_read"));
  if (read == nullptr) { return SQLITE_ERROR; }
  ++blobReads;
  return read(blob, Z, N, iOffset);
}

int main() {
  auto root = (std::filesystem::temp_directory_path() / "outshine-read-plan-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the read-plan fixture owns an isolated directory");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  CheckRead(root + "/assets.sqlite");
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the fixture releases its store before cleanup");
  return Report();
}
