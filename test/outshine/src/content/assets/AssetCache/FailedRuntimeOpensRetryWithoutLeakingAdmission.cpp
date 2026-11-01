#include "RuntimeFixture.h"
#include "Check.h"
#include <array>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <memory>
#include <thread>

int main() {
  using namespace AssetRuntimeFixture;
  const Directory directory;
  Tasks workers(1);
  const auto connections = std::make_shared<JobAdmission>(1);
  const AssetCache::Runtime runtime{.Workers = &workers, .Connections = connections};
  const auto blocker = directory.Path / "blocked";
  std::filesystem::create_directories(directory.Path);
  { const std::ofstream file(blocker); }
  auto cache = AssetCache::Open((blocker / "assets.sqlite").string(), runtime);
  CHECK(cache && connections->Active() == 0, "configuration does not inspect the blocked parent");
  if (!cache) { return Report(); }
  const auto failed = AwaitFind(workers, **cache);
  CHECK(!failed && failed.error() == AssetCacheError::Storage,
        "an actual filesystem failure is not disguised as busy or missing data");
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (connections->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(connections->Active() == 0, "failed opening returns its connection admission");
  std::filesystem::remove(blocker);
  CHECK(AwaitFind(workers, **cache) && connections->Active() == 1,
        "the same configured cache retries successfully after the external error is removed");
  std::atomic<unsigned> failures{0};
  std::array<std::thread, 8> readers;
  for (auto &reader : readers) {
    reader = std::thread([&] {
      for (unsigned attempt = 0; attempt < 32; ++attempt) {
        const auto found = (*cache)->Find(kMissing);
        if (!found || *found) { ++failures; }
      }
    });
  }
  for (auto &reader : readers) { reader.join(); }
  CHECK(failures.load() == 0 && connections->Active() == 1,
        "concurrent readers use the same admitted initialized connection");
  cache->reset();
  while (connections->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(connections->Active() == 0, "the retried cache releases its final connection");
  return Report();
}
