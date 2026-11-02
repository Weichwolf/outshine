#include "RuntimeFixture.h"
#include "Check.h"
#include <chrono>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>

int main() {
  using namespace AssetRuntimeFixture;
  const Directory directory;
  Tasks workers(1);
  const auto connections = std::make_shared<JobAdmission>(1);
  auto held = connections->TryAcquire();
  auto cache = AssetCache::Open((directory.Path / "assets.sqlite").string(),
                                {.Workers = &workers, .Connections = connections});
  CHECK(held && cache && !std::filesystem::exists(directory.Path),
        "a deferred handle does not perform IO or consume the occupied connection");
  if (!cache) { return Report(); }
  const auto deferred = (*cache)->PlanRead(kMissing, 4096);
  CHECK(!deferred && deferred.error() == AssetCacheError::Busy &&
            !std::filesystem::exists(directory.Path),
        "read planning propagates connection admission before touching a mutex or database");
  held.reset();
  std::expected<std::optional<AssetCache::ReadPlan>, AssetCacheError> planned =
      std::unexpected(AssetCacheError::Storage);
  const auto task = workers.Post([&] { planned = (*cache)->PlanRead(kMissing, 4096); });
  const bool completed = task != Tasks::kNoTask && workers.AwaitCompletion(task, 5);
  CHECK(completed, "the first native read plan initializes its connection on the IO worker");
  if (!completed) {
    if (task != Tasks::kNoTask) { workers.Wait(task); }
    return Report();
  }
  CHECK(planned && !*planned && connections->Active() == 1 &&
            std::filesystem::exists(directory.Path),
        "an unopened store becomes a valid cache miss using exactly one admitted connection");
  (void)workers.TakeCompletion(task);
  const auto again = (*cache)->PlanRead(kMissing, 4096);
  CHECK(again && !*again && connections->Active() == 1,
        "subsequent read planning reuses the initialized native connection");
  cache->reset();
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (connections->Active() != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(1ms);
  }
  CHECK(connections->Active() == 0, "retirement returns the lazy connection admission");
  return Report();
}
