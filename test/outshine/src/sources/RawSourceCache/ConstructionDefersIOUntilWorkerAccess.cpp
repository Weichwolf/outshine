#include "RawSourceCache.h"
#include "Check.h"
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;
using namespace outshine::Test;

void Metadata(const std::filesystem::path &root) {
  const auto path = (root / "metadata").string();
  std::fputs("RAW_CACHE_SETUP_BEGIN\n", stderr);
  {
    const RawSourceCache store({.Directory = path});
    const RawSourceCache disabled({.Directory = {}, .Using = RawSourceCache::Use::Off});
    CHECK(store.Directory() == path && store.Enabled() && !disabled.Enabled(),
          "construction captures explicit/default paths and policy without opening storage");
    const auto counters = store.Counters();
    CHECK(counters.Hits == 0 && counters.ReadCalls == 0 && counters.Writes == 0,
          "metadata inspection starts no source work");
  }
  std::fputs("RAW_CACHE_SETUP_END\n", stderr);
  CHECK(!std::filesystem::exists(path),
        "unused enabled stores create no directory or usage database");
}

void Unused(const std::filesystem::path &root) {
  const std::string key(64, 'a');
  const auto miss = (root / "cold-miss").string();
  {
    const RawSourceCache store({.Directory = miss});
    CHECK(store.Lookup(key).Where == RawSourceCache::Presence::Unknown &&
              store.Counters().Misses == 1 && !std::filesystem::exists(miss),
          "a cold read miss creates no cache directory or usage index");
  }
  const auto invalid = (root / "invalid").string();
  const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
  {
    RawSourceCache store({.Directory = invalid, .CapBytes = 3});
    CHECK(!store.Keep("invalid", bytes.data(), 1) && !store.Keep(key, bytes.data(), bytes.size()) &&
              !store.KeepAbsent("invalid") && !std::filesystem::exists(invalid),
          "rejected writes do not initialize storage");
  }
  const auto disabled = (root / "disabled").string();
  {
    RawSourceCache store({.Directory = disabled, .Using = RawSourceCache::Use::Off});
    CHECK(!store.Keep(key, bytes.data(), bytes.size()) && !store.KeepEmpty(key) &&
              store.Lookup(key).Where == RawSourceCache::Presence::Unknown &&
              !std::filesystem::exists(disabled),
          "disabled stores perform no directory/index setup even when used");
  }
}

void Status(const std::filesystem::path &root) {
  const auto path = (root / "status-first").string();
  RawSourceCache store({.Directory = path});
  bool absent = false;
  bool empty = false;
  const std::string absentKey(64, 'b');
  const std::string emptyKey(64, 'c');
  {
    const std::jthread worker([&] {
      absent = store.KeepAbsent(absentKey);
      empty = store.KeepEmpty(emptyKey);
    });
  }
  CHECK(absent && empty, "a first status-only write creates its root on the worker");
  CHECK(store.Lookup(absentKey).Where == RawSourceCache::Presence::Absent &&
            store.Lookup(emptyKey).Where == RawSourceCache::Presence::Bytes &&
            store.Lookup(emptyKey).Bytes.empty(),
        "absence and complete empty bytes remain distinct after lazy initialization");
}

void CheckUsage(const std::filesystem::path &root, size_t sources) {
  sqlite3 *database = nullptr;
  CHECK(
      sqlite3_open_v2((root / ".usage.sqlite").c_str(), &database, SQLITE_OPEN_READONLY, nullptr) ==
          SQLITE_OK,
      "independent usage observer opens after worker shutdown");
  sqlite3_stmt *statement = nullptr;
  const auto prepared = sqlite3_prepare_v2(
      database, "SELECT count(*), sum(stored_bytes) FROM cache_usage_v1", -1, &statement, nullptr);
  CHECK(prepared == SQLITE_OK, "lazy source index has the existing usage schema");
  if (prepared == SQLITE_OK) {
    CHECK(sqlite3_step(statement) == SQLITE_ROW &&
              sqlite3_column_int64(statement, 0) == static_cast<int64_t>(sources) &&
              sqlite3_column_int64(statement, 1) == static_cast<int64_t>(sources * 4),
          "all concurrent publications retain their exact usage records and byte counts");
  }
  (void)sqlite3_finalize(statement);
  CHECK(sqlite3_close(database) == SQLITE_OK, "usage observer closes");
}

void Concurrent(const std::filesystem::path &root, bool recover) {
  constexpr size_t kWorkers = 8;
  const auto path = root / (recover ? "concurrent-recovery" : "concurrent");
  if (recover) { std::ofstream(path) << "keep"; }
  std::array<std::string, kWorkers> keys;
  for (size_t index = 0; index < kWorkers; ++index) {
    keys[index] = std::string(64, "abcdef01"[index]);
  }
  {
    RawSourceCache store({.Directory = path.string()});
    if (recover) {
      const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
      CHECK(!store.Keep(keys[0], bytes.data(), bytes.size()),
            "the first attempt observes the filesystem obstruction");
      std::filesystem::remove(path);
    }
    std::array<bool, kWorkers> kept{};
    std::array<bool, kWorkers> read{};
    std::barrier ready(static_cast<std::ptrdiff_t>(kWorkers));
    {
      std::vector<std::jthread> workers;
      workers.reserve(kWorkers);
      for (size_t index = 0; index < kWorkers; ++index) {
        workers.emplace_back([&, index] {
          const std::array<uint8_t, 4> bytes{static_cast<uint8_t>(index), 11, 17, 23};
          ready.arrive_and_wait();
          kept[index] = store.Keep(keys[index], bytes.data(), bytes.size());
          read[index] =
              store.Lookup(keys[index]).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end());
        });
      }
    }
    for (size_t index = 0; index < kWorkers; ++index) {
      CHECK(kept[index] && read[index], "simultaneous first writers publish and read exact bytes");
    }
    CHECK(std::cmp_equal(store.Counters().Writes, kWorkers) &&
              std::cmp_equal(store.Counters().Hits, kWorkers),
          "concurrent initialization preserves source counters");
  }
  CheckUsage(path, kWorkers);
}

void Unavailable(const std::filesystem::path &root) {
  const auto path = root / "foreign";
  { std::ofstream(path) << "keep"; }
  {
    RawSourceCache store({.Directory = path.string()});
    const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
    CHECK(!store.Keep(std::string(64, 'a'), bytes.data(), bytes.size()) &&
              store.Counters().WriteFailures == 1 && std::filesystem::file_size(path) == 4,
          "unavailable storage reports write failure while preserving the foreign file");
    std::filesystem::remove(path);
    const std::string key(64, 'a');
    CHECK(store.Keep(key, bytes.data(), bytes.size()) &&
              store.Lookup(key).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
          "the same store recovers after the filesystem obstruction is removed");
  }
  CheckUsage(path, 1);
}

void IndexRecovery(const std::filesystem::path &root) {
  const auto path = root / "blocked-index";
  std::filesystem::create_directories(path / ".usage.sqlite");
  const std::string key(64, 'b');
  const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
  {
    RawSourceCache store({.Directory = path.string()});
    CHECK(store.Keep(key, bytes.data(), bytes.size()),
          "source bytes remain available when the usage index cannot open");
    std::filesystem::remove(path / ".usage.sqlite");
    CHECK(store.Lookup(key).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
          "a later cache hit reopens the repaired usage index");
  }
  CheckUsage(path, 1);
}
}

int main() {
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-lazy-source-cache-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache parent created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const std::filesystem::path root(directory);
  Metadata(root);
  Unused(root);
  Status(root);
  Concurrent(root, false);
  Concurrent(root, true);
  Unavailable(root);
  IndexRecovery(root);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "isolated cache inputs removed");
  return Report();
}
