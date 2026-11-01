#include "AssetCache.h"
#include "SqliteFileBudget.h"
#include "Sha256.h"
#include "Check.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <string_view>
#include <memory>
#include <span>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint64_t kLimit = uint64_t{12} * size_t{1024} * 1024;

struct Directory {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-auto-budget-" +
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
  std::vector<uint8_t> result(size_t{128} * 1024);
  uint64_t state = id + 1;
  for (auto &byte : result) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    byte = static_cast<uint8_t>(state);
  }
  return result;
}

auto Prepare(AssetCache &cache, uint64_t id) {
  auto bytes = Payload(id);
  const AssetRecord record{.Key = Key(id),
                           .Kind = "budget-test",
                           .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                           .Package = {},
                           .ByteCount = bytes.size(),
                           .Parent = {}};
  return cache.PreparePublication({record}, std::move(bytes));
}

bool Publish(AssetCache &cache, uint64_t id) {
  const auto prepared = Prepare(cache, id);
  CHECK(prepared, "a complete immutable package is prepared once");
  if (!prepared) { return false; }
  for (int step = 0; step < 1024; ++step) {
    const auto published = cache.Publish(*prepared);
    if (published) { return true; }
    if (published.error() != AssetCacheError::Busy) {
      CHECK(false, "budget maintenance must not convert a recoverable full cache into failure");
      return false;
    }
  }
  CHECK(false, "bounded reclamation retries must make forward progress");
  return false;
}

int Scalar(sqlite3 *db, const char *sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) { return -1; }
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
  return sqlite3_step(raw) == SQLITE_ROW ? sqlite3_column_int(raw, 0) : -1;
}

void Pinned() {
  const Directory directory;
  CHECK(SqliteFileBudget::Configure(directory.Path, kLimit),
        "a small native cache budget persists");
  const auto budget = SqliteFileBudget::Open(directory.Path);
  auto cache = AssetCache::Open((directory.Path / "assets.sqlite").string());
  CHECK(budget && cache, "the actual cache consumes the shared native budget");
  if (!budget || !cache) { return; }
  for (uint64_t id = 0; id < 8; ++id) {
    if (!Publish(**cache, id)) { return; }
  }
  auto lease = (*cache)->Load(Key(0), size_t{1024} * 1024);
  CHECK(lease && *lease, "a native product lease is retained across later eviction");
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open_v2((directory.Path / "assets.sqlite").c_str(),
                                     &raw,
                                     SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,
                                     (*budget)->VfsName());
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> reader(raw, sqlite3_close);
  CHECK(opened == SQLITE_OK && sqlite3_exec(raw, "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK &&
            Scalar(raw, "SELECT count(*) FROM packages") == 8,
        "a real reader pins the current cache snapshot");
  uint64_t pending = 8;
  for (; pending < 128; ++pending) {
    const auto prepared = Prepare(**cache, pending);
    CHECK(prepared, "a pending package is valid");
    if (!prepared) { return; }
    const auto published = (*cache)->Publish(*prepared);
    if (!published) {
      CHECK(published.error() == AssetCacheError::Busy,
            "pinned capacity produces recoverable backpressure");
      break;
    }
  }
  CHECK(pending < 128 && Scalar(raw, "SELECT count(*) FROM packages") == 8,
        "automatic cleanup cannot revoke the held reader or grow WAL without limit");
  const auto absent = (*cache)->Find(Key(pending));
  CHECK(absent && !*absent, "a blocked publication exposes no partial product");
  CHECK(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the reader releases its snapshot");
  for (uint64_t id = pending; id < 160; ++id) {
    if (!Publish(**cache, id)) { return; }
  }
  const auto usage = (*budget)->Inspect();
  CHECK(usage && usage->ChargedBytes <= kLimit,
        "automatic eviction keeps real cache files within budget");
  CHECK(lease && *lease && std::ranges::equal((**lease).Bytes(), Payload(0)),
        "the held native product survives removal of its persisted package");
  const auto newest = (*cache)->Load(Key(159), size_t{1024} * 1024);
  CHECK(newest && *newest && std::ranges::equal((**newest).Bytes(), Payload(159)),
        "the newest product loads with exact original bytes after repeated reclamation");
}

void TwoBanks() {
  const Directory directory;
  CHECK(SqliteFileBudget::Configure(directory.Path, kLimit), "one limit covers both native banks");
  auto main = AssetCache::Open((directory.Path / "assets.sqlite").string());
  auto prototypes = AssetCache::Open((directory.Path / "prototypes.sqlite").string());
  CHECK(main && prototypes, "two actual native stores share one root");
  if (!main || !prototypes) { return; }
  for (uint64_t id = 0; id < 40; ++id) {
    if (!Publish(**main, id)) { return; }
  }
  sqlite3 *raw = nullptr;
  CHECK(sqlite3_open((directory.Path / "assets.sqlite").c_str(), &raw) == SQLITE_OK,
        "recency setup opens only the private test bank");
  {
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    CHECK(sqlite3_exec(
              raw, "UPDATE cache_usage_v1 SET last_used_unix_s=0", nullptr, nullptr, nullptr) ==
              SQLITE_OK,
          "the main bank is explicitly older than upcoming prototype demand");
  }
  for (uint64_t id = 1000; id < 1080; ++id) {
    if (!Publish(**prototypes, id)) { return; }
  }
  const auto old = (*main)->Find(Key(0));
  const auto latest = (*prototypes)->Load(Key(1079), size_t{1024} * 1024);
  CHECK(old && !*old && latest && *latest,
        "prototype publication reclaims cold data from the other bank instead of stalling");
  const auto budget = SqliteFileBudget::Open(directory.Path);
  const auto usage = budget ? (*budget)->Inspect() : std::unexpected(std::string("missing budget"));
  CHECK(usage && usage->ChargedBytes <= kLimit,
        "both databases and their sidecars share the file limit");
}
}

int main() {
  Pinned();
  TwoBanks();
  return outshine::Test::Report();
}
