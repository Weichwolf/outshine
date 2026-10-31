#include "AssetCache.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <print>
#include <ratio>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
bool failWrite = false;
unsigned partialWrites = 0;

AssetRecord Record(size_t size) {
  return {.Key = std::string(64, 'a'),
          .Kind = "incremental-publication",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = {},
          .OffsetBytes = 0,
          .ByteCount = size,
          .Parent = {},
          .RequestKey = std::string(64, 'b')};
}

void LargePackage(const std::string &path) {
  auto store = AssetCache::Open(path);
  CHECK(store, "a real store opens before measuring SQLite publication memory");
  if (!store) { return; }
  constexpr size_t size = size_t{16} * 1024 * 1024;
  std::vector<uint8_t> input(size);
  uint32_t random = 0x9e3779b9;
  for (auto &byte : input) {
    random ^= random << 13u;
    random ^= random >> 17u;
    random ^= random << 5u;
    byte = static_cast<uint8_t>(random);
  }
  auto prepared = (*store)->PreparePublication({Record(size)}, input);
  CHECK(prepared, "an incompressible native package is prepared before observing SQL allocations");
  if (!prepared) { return; }
  sqlite3_int64 baseline = 0;
  sqlite3_int64 peak = 0;
  CHECK(sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &baseline, &peak, 1) == SQLITE_OK &&
            baseline > 0,
        "SQLite reports its actual process allocation baseline and resets the high-water mark");
  const auto began = std::chrono::steady_clock::now();
  const auto published = (*store)->Publish(*prepared);
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  sqlite3_int64 current = 0;
  CHECK(sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &current, &peak, 0) == SQLITE_OK,
        "SQLite exposes the terminal publication high-water mark");
  std::println(
      "SQL_PUBLICATION native_bytes={} baseline_bytes={} peak_bytes={} extra_bytes={} ms={}",
      size,
      baseline,
      peak,
      peak - baseline,
      elapsed);
  CHECK(published && peak >= baseline && std::cmp_less(peak - baseline, size),
        "publication never allocates an additional native-sized SQL record buffer");
  CHECK(sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &baseline, &peak, 1) == SQLITE_OK,
        "metadata planning starts from a fresh SQLite allocation baseline");
  const auto plan = (*store)->PlanRead(std::string(64, 'a'), size);
  CHECK(sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &current, &peak, 0) == SQLITE_OK && plan &&
            *plan && (**plan).PackageBytesMost == size && std::cmp_less(peak - baseline, size),
        "metadata planning never materializes either inline or streamed package bodies");
  const auto loaded = (*store)->Load(std::string(64, 'a'), size);
  CHECK(loaded && *loaded && std::ranges::equal((**loaded).Bytes(), input),
        "the complete large package keeps its bytes and checksum after incremental publication");
}

void FailedWrite(const std::string &path, bool replacePackage) {
  auto store = AssetCache::Open(path);
  CHECK(store, "a real store opens for a transactional partial-write opponent");
  if (!store) { return; }
  const std::vector<uint8_t> original(4096, uint8_t{7});
  const std::array records{Record(original.size())};
  const std::string request(64, 'b');
  CHECK((*store)->Publish(records, original), "the original package and request binding commit");
  const auto before = (*store)->Find(records.front().Key);
  CHECK(before && *before, "the old committed package identity is observable");
  if (!before || !*before) { return; }
  auto replacement = original;
  if (replacePackage) { std::ranges::fill(replacement, uint8_t{23}); }
  auto prepared = (*store)->PreparePublication({records.front()}, replacement);
  CHECK(prepared, "the retryable replacement is owned before entering its write transaction");
  if (!prepared) { return; }
  failWrite = true;
  partialWrites = 0;
  const auto failed = (*store)->Publish(*prepared);
  failWrite = false;
  CHECK(!failed && failed.error() == AssetCacheError::Storage && partialWrites == 1,
        "a real partial blob write surfaces its IO failure without committing");
  auto observer = AssetCache::Open(path);
  CHECK(observer, "a fresh independent reader observes the rollback boundary");
  if (!observer) { return; }
  const auto preserved = (*observer)->LoadRequest(request, original.size());
  CHECK(preserved && *preserved && (**preserved).Record().Package == (**before).Package &&
            std::ranges::equal((**preserved).Bytes(), original),
        "failed overwrites and failed rebindings preserve the complete old offline product");
  CHECK((*store)->Publish(*prepared), "the same owned prepared package retries after rollback");
  const auto winner = (*observer)->LoadRequest(request, replacement.size());
  CHECK(winner && *winner && std::ranges::equal((**winner).Bytes(), replacement),
        "the successful retry atomically exposes the complete replacement and its binding");
}
}

extern "C" int sqlite3_blob_write(sqlite3_blob *blob, const void *z, int n, int iOffset) {
  using Write = int (*)(sqlite3_blob *, const void *, int, int);
  static const auto write = reinterpret_cast<Write>(dlsym(RTLD_NEXT, "sqlite3_blob_write"));
  if (write == nullptr) { return SQLITE_IOERR; }
  if (failWrite) {
    failWrite = false;
    ++partialWrites;
    const int partial = write(blob, z, std::max(1, n / 2), iOffset);
    return partial == SQLITE_OK ? SQLITE_IOERR : partial;
  }
  return write(blob, z, n, iOffset);
}

int main() {
  auto root = (std::filesystem::temp_directory_path() / "outshine-sql-publication-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "SQL opponents own an isolated directory");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  LargePackage(root + "/large.sqlite");
  FailedWrite(root + "/overwrite.sqlite", false);
  FailedWrite(root + "/rebind.sqlite", true);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the SQL fixtures close their stores before cleanup");
  return Report();
}
