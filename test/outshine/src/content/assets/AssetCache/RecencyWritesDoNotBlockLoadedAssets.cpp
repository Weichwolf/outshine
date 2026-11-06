#include "RuntimeFixture.h"
#include "AssetCache.h"
#include "AssetRecord.h"
#include "Check.h"
#include "JobAdmission.h"
#include "Tasks.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <dlfcn.h>
#include <format>
#include <memory>
#include <semaphore>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <thread>

namespace {
using namespace AssetRuntimeFixture;
constexpr unsigned kAssets = 513;

struct Observation {
  std::string Path;
  std::thread::id Reader;
  std::binary_semaphore Entered{0}, Release{0}, ReadFinished{0};
  std::atomic_bool Armed{true}, SeparateWriter{false};
  std::atomic_uint Loaded{0};
};

std::atomic<Observation *> Hook{nullptr};

std::string Key(unsigned i) {
  return std::format("{:064x}", i);
}

int64_t Query(const std::string &path, const char *sql) {
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open(path.c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  CHECK(opened == SQLITE_OK, "the saved recency state opens independently");
  if (opened != SQLITE_OK) { return -1; }
  sqlite3_stmt *prepared = nullptr;
  const int ready = sqlite3_prepare_v2(database.get(), sql, -1, &prepared, nullptr);
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(prepared,
                                                                             &sqlite3_finalize);
  CHECK(ready == SQLITE_OK, "the saved recency query prepares");
  if (ready != SQLITE_OK) { return -1; }
  const int result = sqlite3_step(statement.get());
  CHECK(result == SQLITE_ROW || result == SQLITE_DONE, "the saved recency query completes");
  if (result == SQLITE_ROW) { return sqlite3_column_int64(statement.get(), 0); }
  return result == SQLITE_DONE ? 0 : -1;
}

bool Prepare(const std::string &path) {
  auto cache = AssetCache::Open(path);
  if (!cache) { return false; }
  for (unsigned i = 0; i < kAssets; ++i) {
    const std::array<uint8_t, 4> bytes{
        static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8u), 13, 47};
    const AssetRecord record{.Key = Key(i),
                             .Kind = "game-assets",
                             .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                             .Package = {},
                             .ByteCount = bytes.size(),
                             .Parent = {}};
    if (!(*cache)->Publish(std::span(&record, 1), bytes)) { return false; }
  }
  return true;
}

int Trace(unsigned event, void * /*context*/, void *statement, void * /*detail*/) {
  auto *observation = Hook.load();
  if (event != SQLITE_TRACE_STMT || observation == nullptr) { return 0; }
  const char *sql = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
  if (sql != nullptr &&
      std::string_view(sql).starts_with(
          "INSERT INTO cache_usage_v1(key,stored_bytes,created_unix_s,"
          "last_used_unix_s) VALUES(?,?,?,?)") &&
      observation->Armed.exchange(false)) {
    observation->SeparateWriter.store(std::this_thread::get_id() != observation->Reader);
    observation->Entered.release();
    (void)observation->Release.try_acquire_for(5s);
  }
  return 0;
}

void Stream(const std::string &path) {
  Observation observation{.Path = path, .Reader = {}};
  Hook.store(&observation);
  {
    Tasks workers(2);
    const auto connections = std::make_shared<JobAdmission>(1);
    auto cache = AssetCache::Open(path, {.Workers = &workers, .Connections = connections});
    CHECK(cache, "the scenario opens a runtime asset bank");
    if (!cache) {
      Hook.store(nullptr);
      return;
    }
    const auto reader = workers.Post([&] {
      observation.Reader = std::this_thread::get_id();
      for (unsigned i = 0; i < kAssets; ++i) {
        const auto loaded = (*cache)->Load(Key(i), 4);
        if (loaded && *loaded && (**loaded).Bytes().size() == 4 &&
            (**loaded).Bytes().front() == static_cast<uint8_t>(i)) {
          ++observation.Loaded;
        }
      }
      observation.ReadFinished.release();
    });
    const bool entered = observation.Entered.try_acquire_for(5s);
    CHECK(entered, "a real recency batch reaches SQLite while game assets stream");
    const bool complete = observation.ReadFinished.try_acquire_for(5s);
    CHECK(complete && observation.Loaded == kAssets,
          "every saved asset loads while the recency writer is stopped");
    CHECK(observation.SeparateWriter, "asset reads do not execute recency writes inline");
    observation.Release.release();
    workers.Wait(reader);
    cache->reset();
    const auto until = std::chrono::steady_clock::now() + 5s;
    while (connections->Active() != 0 && std::chrono::steady_clock::now() < until) {
      std::this_thread::yield();
    }
    CHECK(connections->Active() == 0, "the bank retires its connections after draining metadata");
  }
  Hook.store(nullptr);
}
}

extern "C" int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) {
  using Open = int (*)(const char *, sqlite3 **, int, const char *);
  static const auto open = reinterpret_cast<Open>(dlsym(RTLD_NEXT, "sqlite3_open_v2"));
  if (open == nullptr) { return SQLITE_CANTOPEN; }
  const int result = open(filename, ppDb, flags, zVfs);
  const auto *observation = Hook.load();
  if (result == SQLITE_OK && observation != nullptr && filename != nullptr &&
      filename == observation->Path) {
    (void)sqlite3_trace_v2(*ppDb, SQLITE_TRACE_STMT, Trace, nullptr);
  }
  return result;
}

int main() {
  const Directory directory;
  const auto path = (directory.Path / "assets.sqlite").string();
  const bool prepared = Prepare(path);
  CHECK(prepared, "the saved world has more distinct assets than two recency batches");
  if (prepared) {
    CHECK(Query(path, "UPDATE cache_usage_v1 SET last_used_unix_s=1") == 0,
          "the revisited assets were last used in an earlier game session");
    Stream(path);
    CHECK(Query(path, "SELECT count(*) FROM cache_usage_v1 WHERE last_used_unix_s>1") >= 512,
          "both retained recency batches persist after the scenario closes");
  }
  return Report();
}
