#include "UrbanAssetFixture.h"
#include "AssetCache.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <semaphore>
#include <thread>
#include <utility>

namespace {
using namespace UrbanAssetFixture;
using namespace outshine::Test;
constexpr auto kDeadline = std::chrono::seconds(3);

enum class Stage { Idle, Lookup, Generation };

class WorkerBlock {
public:
  ~WorkerBlock() { Release(); }

  [[nodiscard]] bool Hold(Tasks &pool) {
    Release();
    Signal_ = std::make_shared<Signal>();
    if (!pool.PostDetached([signal = Signal_] {
          signal->Entered.release();
          signal->Release.acquire();
        })) {
      Signal_.reset();
      return false;
    }
    return Signal_->Entered.try_acquire_for(kDeadline);
  }

  void Release() {
    if (const auto signal = std::exchange(Signal_, {})) { signal->Release.release(); }
  }

private:
  struct Signal {
    std::binary_semaphore Entered{0}, Release{0};
  };

  std::shared_ptr<Signal> Signal_;
};

void Barrier(Tasks &pool) {
  const auto task = pool.Post([] {});
  CHECK(task != Tasks::kNoTask, "the pool accepts a terminal barrier");
  if (task != Tasks::kNoTask) { pool.Wait(task); }
}

void Drain(Tasks &pool, const std::weak_ptr<SourceSession> &lifetime) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do { Barrier(pool); } while (!lifetime.expired() && std::chrono::steady_clock::now() < until);
}

void Retire(const std::filesystem::path &directory, Stage stage) {
  Fixture fixture(directory, Availability::Ready);
  if (!fixture.Urban) { return; }
  WorkerBlock held;
  bool blocked = held.Hold(fixture.Compute);
  CHECK(blocked, "the worker is held outside the urban frontend");
  if (!blocked) { return; }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  if (stage != Stage::Idle) {
    CHECK(fixture.Urban->Acquire(request), "lookup is admitted while the worker is held");
  }
  if (stage == Stage::Generation) {
    bool queued = false;
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    do {
      held.Release();
      Barrier(fixture.Compute);
      blocked = held.Hold(fixture.Compute);
      CHECK(blocked, "the worker can be held between native source steps");
      if (!blocked) { break; }
      const auto result = fixture.Urban->Acquire(request);
      CHECK(result && result->Status == PreparedUrbanAreas::State::Pending,
            "native source advancement leaves generation queued behind the held worker");
      const auto costs = fixture.Urban->Costs();
      queued = costs.SourcePolls > 0 && fixture.Tiles->AdmissionCounts().Tickets == 0;
    } while (!queued && std::chrono::steady_clock::now() < until);
    CHECK(queued && fixture.Calls == 2 && fixture.Urban->Costs().Generations == 0,
          "both native source products are delivered before generation can execute");
  }
  const std::weak_ptr<SourceSession> lifetime = fixture.Session;
  fixture.Session.reset();
  fixture.Tiles.reset();
  std::binary_semaphore retired(0);
  std::thread release([urban = std::move(fixture.Urban), &retired] mutable {
    urban.reset();
    retired.release();
  });
  CHECK(retired.try_acquire_for(std::chrono::milliseconds(250)),
        "idle, lookup and generation retirement return before the worker is released");
  CHECK(!lifetime.expired(), "queued work and cleanup keep the source session alive");
  held.Release();
  release.join();
  Drain(fixture.Compute, lifetime);
  CHECK(lifetime.expired(), "the last worker releases all retired source owners");
  if (stage == Stage::Generation) {
    auto cache = AssetCache::Open((directory / "assets.sqlite").string());
    CHECK(cache, "the finished cache can be inspected after canceled generation");
    if (!cache) { return; }
    constexpr double extentM = 10'000'000;
    const auto records = (*cache)->Select(
        {.Bounds = {.Min = {{-extentM, -extentM, -extentM}}, .Max = {{extentM, extentM, extentM}}},
         .Kind = "urban-area",
         .Level = {},
         .Radius = {},
         .Planes = {}});
    CHECK(records && records->empty(), "retired queued generation publishes no urban asset");
  }
}

void Discard(const std::filesystem::path &directory) {
  auto fixture = std::make_unique<Fixture>(directory, Availability::Ready);
  if (!fixture->Urban) { return; }
  WorkerBlock held;
  CHECK(held.Hold(fixture->Compute), "the worker is held before queue discard");
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  CHECK(fixture->Urban->Acquire(request), "lookup can be queued before shutdown");
  const std::weak_ptr<SourceSession> lifetime = fixture->Session;
  fixture->Session.reset();
  fixture->Tiles.reset();
  std::binary_semaphore retired(0);
  std::thread retirement([urban = std::move(fixture->Urban), &retired] mutable {
    urban.reset();
    retired.release();
  });
  const bool returned = retired.try_acquire_for(std::chrono::milliseconds(250));
  CHECK(returned, "retirement returns before queue shutdown");
  if (!returned) { held.Release(); }
  retirement.join();
  if (!returned) { return; }
  CHECK(!lifetime.expired(), "queued ownership survives the frontend");
  std::atomic_bool stopped{false};
  std::thread stop([&] {
    fixture.reset();
    stopped = true;
  });
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (!lifetime.expired() && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(lifetime.expired() && !stopped,
        "shutdown discards queued ownership before joining the held worker");
  held.Release();
  stop.join();
  CHECK(stopped, "shutdown completes after its only running worker is released");
}

void Completion(const std::filesystem::path &directory) {
  Fixture fixture(directory, Availability::Ready);
  if (!fixture.Urban) { return; }
  const auto unrelated = fixture.Compute.Post([] {});
  CHECK(fixture.Compute.AwaitCompletion(unrelated, 1), "an unrelated completion remains retained");
  WorkerBlock held;
  CHECK(held.Hold(fixture.Compute), "the worker is held before the requested lookup");
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  CHECK(fixture.Urban->Acquire(request), "the actual lookup is queued");
  CHECK(!fixture.Urban->AwaitProgress(0) && !fixture.Urban->AwaitProgress(0.002),
        "an unrelated retained completion cannot complete the requested urban wait");
  std::binary_semaphore entered(0);
  std::binary_semaphore finished(0);
  std::atomic_bool progress{false};
  std::thread waiter([&] {
    entered.release();
    progress = fixture.Urban->AwaitProgress(2);
    finished.release();
  });
  CHECK(entered.try_acquire_for(kDeadline), "the waiter starts while its job is held");
  CHECK(!finished.try_acquire_for(std::chrono::milliseconds(2)),
        "the waiter cannot return before its own pending lookup can execute");
  held.Release();
  waiter.join();
  CHECK(progress, "the actual detached job wakes the waiter");
  Barrier(fixture.Compute);
  CHECK(fixture.Urban->AwaitProgress(1), "completion before waiting remains observable");
  CHECK(fixture.Compute.TakeCompletion(unrelated), "unrelated tracked handles keep their owner");
}
}

int main() {
  const std::filesystem::path directory = PlantedPath("urban-retirement");
  Retire(directory / "idle", Stage::Idle);
  Retire(directory / "lookup", Stage::Lookup);
  Retire(directory / "generation", Stage::Generation);
  Discard(directory / "discard");
  Completion(directory / "completion");
  std::filesystem::remove_all(directory);
  return Report();
}
