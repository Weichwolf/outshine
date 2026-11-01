#include "AssetCache.h"
#include "AssetCacheReclaimer.h"
#include "Check.h"
#include "Sha256.h"
#include "SqliteFileBudget.h"
#include "Tasks.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <print>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint64_t kMiB = uint64_t{1024} * 1024;

struct Directory {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-idle-budget-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  Directory() { std::filesystem::create_directories(Path); }

  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

std::string Key(uint64_t id) {
  return Sha256Hex(&id, sizeof(id));
}

std::vector<uint8_t> Payload(uint64_t id) {
  std::vector<uint8_t> bytes(size_t{128} * 1024);
  uint64_t state = id + 1;
  for (auto &byte : bytes) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    byte = static_cast<uint8_t>(state);
  }
  return bytes;
}

bool Wait(AssetCacheReclaimer &maintenance, AssetCacheReclaimer::Status wanted) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(8);
  do {
    maintenance.Advance();
    if (maintenance.Inspect().Phase == wanted) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < end);
  return false;
}

void Shrink() {
  const Directory directory;
  CHECK(SqliteFileBudget::Configure(directory.Path, 24 * kMiB), "the initial cache has room");
  const auto path = (directory.Path / "assets.sqlite").string();
  auto cache = AssetCache::Open(path);
  CHECK(cache, "the native cache opens");
  if (!cache) { return; }
  for (uint64_t id = 0; id < 80; ++id) {
    const auto bytes = Payload(id);
    const AssetRecord record{.Key = Key(id),
                             .Kind = "idle-budget",
                             .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                             .Package = {},
                             .ByteCount = bytes.size(),
                             .Parent = {}};
    if (const auto result = (*cache)->Publish({&record, 1}, bytes); !result) {
      CHECK(false, "the old working set publishes before the limit changes");
      return;
    }
  }
  cache->reset();
  CHECK(SqliteFileBudget::Configure(directory.Path, 8 * kMiB),
        "a smaller limit persists only after users close");
  cache = AssetCache::Open(path);
  const auto budget = SqliteFileBudget::Open(directory.Path);
  CHECK(cache && budget, "the oversized bank reopens without replacement");
  if (!cache || !budget) { return; }
  const auto held = (*cache)->Load(Key(0), kMiB);
  CHECK(held && *held, "a loaded product is retained independently of disk eviction");
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE, (*budget)->VfsName()) ==
            SQLITE_OK,
        "an independent connection shares the real file budget");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  if (raw == nullptr) { return; }
  {
    const SqliteFileBudget::Maintenance allowance(**budget);
    const auto sql = "UPDATE cache_usage_v1 SET last_used_unix_s=CASE WHEN key='" + Key(0) +
                     "' THEN 0 WHEN key='" + Key(79) +
                     "' THEN 9223372036854775806 ELSE 2 END;PRAGMA wal_checkpoint(TRUNCATE)";
    CHECK(sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
          "recency separates the cold lease from the most recent retained product");
  }
  sqlite3 *readerRaw = nullptr;
  CHECK(sqlite3_open_v2(path.c_str(), &readerRaw, SQLITE_OPEN_READONLY, (*budget)->VfsName()) ==
            SQLITE_OK,
        "the old reader opens on the same native bank");
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> reader(readerRaw, sqlite3_close);
  if (readerRaw == nullptr) { return; }
  CHECK(sqlite3_exec(readerRaw, "BEGIN;SELECT count(*) FROM packages", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "a real reader pins the old snapshot");
  {
    const SqliteFileBudget::Maintenance allowance(**budget);
    CHECK(sqlite3_exec(raw,
                       "UPDATE cache_usage_v1 SET last_used_unix_s=last_used_unix_s+1",
                       nullptr,
                       nullptr,
                       nullptr) == SQLITE_OK,
          "a later WAL frame makes the old snapshot block truncation");
  }
  Tasks workers(1);
  AssetCacheReclaimer maintenance(workers, directory.Path.string());
  CHECK(Wait(maintenance, AssetCacheReclaimer::Status::Deferred),
        "maintenance defers instead of waiting on a retained reader");
  CHECK(sqlite3_exec(readerRaw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the old snapshot releases");
  const bool reclaimed = Wait(maintenance, AssetCacheReclaimer::Status::Idle);
  const auto status = maintenance.Inspect();
  if (!reclaimed) {
    std::println(stderr,
                 "reclaimer phase={} error={} steps={} charge={} limit={}",
                 static_cast<unsigned>(status.Phase),
                 status.Error ? static_cast<int>(*status.Error) : -1,
                 status.Steps,
                 status.ChargedBytes,
                 status.LimitBytes);
  }
  CHECK(reclaimed, "read-only demand resumes maintenance without another publication");
  const auto usage = (*budget)->Inspect();
  CHECK(usage && usage->ChargedBytes + usage->WalBytes + (*budget)->Reserve() < 8 * kMiB,
        "the actual files shrink with checkpoint headroom inside the new shared limit");
  const auto cold = (*cache)->Find(Key(0));
  const auto latest = (*cache)->Load(Key(79), kMiB);
  CHECK(cold && !*cold && latest && *latest,
        "indexed recency removes the cold package and preserves the recently used product");
  CHECK(held && *held && std::ranges::equal((**held).Bytes(), Payload(0)),
        "an evicted on-disk package does not invalidate already loaded bytes");
  CHECK(status.Steps > 1 && status.LimitBytes == 8 * kMiB && status.ChargedBytes <= 8 * kMiB,
        "compact atomic diagnostics describe actual incremental work");
}
}

int main() {
  Shrink();
  return outshine::Test::Report();
}
