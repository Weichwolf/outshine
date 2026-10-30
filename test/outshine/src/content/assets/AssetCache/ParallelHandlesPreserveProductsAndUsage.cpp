#include "AssetCache.h"
#include "Check.h"
#include "Sha256.h"
#include "Tasks.h"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <sqlite3.h>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-parallel-cache-handles-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  constexpr unsigned kWorkers = 6;
  constexpr unsigned kProductsPerWorker = 8;
  std::mutex startMutex;
  std::condition_variable started;
  unsigned arrived = 0;
  std::atomic<unsigned> completed{0};
  {
    Tasks workers(kWorkers);
    std::vector<Tasks::Handle> jobs;
    jobs.reserve(kWorkers);
    for (unsigned worker = 0; worker < kWorkers; ++worker) {
      jobs.push_back(workers.Post([&, worker] {
        {
          std::unique_lock lock(startMutex);
          ++arrived;
          started.notify_all();
          started.wait(lock, [&] { return arrived == kWorkers; });
        }
        const auto path =
            worker % 2 == 0 ? directory / "assets.sqlite" : directory / "." / "assets.sqlite";
        auto cache = AssetCache::Open(path.string());
        if (!cache) { return; }
        for (unsigned item = 0; item < kProductsPerWorker; ++item) {
          const auto identity = worker * kProductsPerWorker + item;
          const auto key = Sha256Hex(&identity, sizeof(identity));
          const std::array<uint8_t, 4> payload{static_cast<uint8_t>(identity), 23, 31, 47};
          const AssetRecord record{.Key = key,
                                   .Kind = "parallel-test",
                                   .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                   .Package = {},
                                   .ByteCount = payload.size(),
                                   .Parent = {}};
          if (!(*cache)->Publish(std::span(&record, 1), payload)) { continue; }
          const auto loaded = (*cache)->Load(key, payload.size());
          if (loaded && *loaded && (**loaded).Bytes().size() == payload.size() &&
              (**loaded).Bytes().front() == payload.front()) {
            ++completed;
          }
        }
      }));
    }
    for (const auto job : jobs) { workers.Wait(job); }
  }
  CHECK(completed == kWorkers * kProductsPerWorker,
        "parallel handles of one database preserve every independently prepared product");
  const auto path = (directory / "assets.sqlite").string();
  {
    const auto reopened = AssetCache::Open(path);
    CHECK(reopened, "the concurrent cache reopens after all worker handles retire");
  }
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
        "the persisted usage ledger opens independently");
  sqlite3_stmt *usage = nullptr;
  CHECK(sqlite3_prepare_v2(database, "SELECT count(*) FROM cache_usage_v1", -1, &usage, nullptr) ==
            SQLITE_OK,
        "the persisted usage table remains readable");
  if (usage != nullptr) {
    CHECK(sqlite3_step(usage) == SQLITE_ROW &&
              sqlite3_column_int64(usage, 0) == sqlite3_int64{kWorkers} * kProductsPerWorker,
          "every parallel package retains its persistent usage observation");
  }
  sqlite3_finalize(usage);
  sqlite3_close(database);
  std::filesystem::remove_all(directory);
  return Report();
}
