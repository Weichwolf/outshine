#include "RuntimeFixture.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <atomic>
#include <memory>
#include <thread>

int main() {
  using namespace AssetRuntimeFixture;
  const Directory directory;
  Tasks workers(1);
  const auto connections = std::make_shared<JobAdmission>(1);
  auto cache = AssetCache::Open((directory.Path / "assets.sqlite").string(),
                                {.Workers = &workers, .Connections = connections});
  CHECK(cache, "a shared runtime handle can be configured without opening its database");
  if (!cache) { return Report(); }
  std::array<std::thread, 8> readers;
  std::atomic<unsigned> completed{0};
  std::atomic<unsigned> failures{0};
  const auto until = std::chrono::steady_clock::now() + 5s;
  for (auto &reader : readers) {
    reader = std::thread([&] {
      for (;;) {
        const auto found = (*cache)->Find(kMissing);
        if (found) {
          if (*found) { ++failures; }
          ++completed;
          return;
        }
        if (found.error() != AssetCacheError::Busy || std::chrono::steady_clock::now() >= until) {
          ++failures;
          return;
        }
        std::this_thread::yield();
      }
    });
  }
  for (auto &reader : readers) { reader.join(); }
  CHECK(completed.load() == readers.size() && failures.load() == 0 && connections->Active() == 1,
        "concurrent first IO initializes exactly one admitted connection and all readers finish");
  cache->reset();
  while (connections->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(connections->Active() == 0, "the shared runtime handle retires its one connection");
  return Report();
}
