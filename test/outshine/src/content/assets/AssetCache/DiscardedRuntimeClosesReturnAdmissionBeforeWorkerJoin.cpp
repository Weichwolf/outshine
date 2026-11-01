#include "RuntimeFixture.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <semaphore>
#include <thread>

int main() {
  using namespace AssetRuntimeFixture;
  const Directory directory;
  auto workers = std::make_unique<Tasks>(1);
  const auto connections = std::make_shared<JobAdmission>(1);
  auto cache = AssetCache::Open((directory.Path / "assets.sqlite").string(),
                                {.Workers = workers.get(), .Connections = connections});
  CHECK(cache && AwaitFind(*workers, **cache) && connections->Active() == 1,
        "an actual runtime connection owns admission before shutdown");
  if (!cache) { return Report(); }
  std::binary_semaphore entered(0);
  std::binary_semaphore release(0);
  const bool posted = workers->PostDetached([&] {
    entered.release();
    release.acquire();
  });
  CHECK(posted && entered.try_acquire_for(5s), "the actual worker is held before retirement");
  cache->reset();
  CHECK(connections->Active() == 1, "queued close keeps its real connection admission");
  std::atomic<bool> stopped{false};
  std::thread shutdown([&] {
    workers.reset();
    stopped.store(true);
  });
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (connections->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  CHECK(connections->Active() == 0 && !stopped.load(),
        "discarded close releases SQLite and admission before the held worker is joined");
  release.release();
  shutdown.join();
  CHECK(stopped.load(), "shutdown completes after the running worker is released");
  return Report();
}
