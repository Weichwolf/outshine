#include "AssetCacheReclaimer.h"
#include "Check.h"
#include "Tasks.h"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto path = std::filesystem::temp_directory_path() /
                    ("outshine-cancel-budget-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Tasks workers(1);
  std::mutex mutex;
  std::condition_variable wake;
  bool entered = false;
  bool released = false;
  const auto blocker = workers.Post([&] {
    std::unique_lock lock(mutex);
    entered = true;
    wake.notify_all();
    (void)wake.wait_for(lock, std::chrono::seconds(5), [&] { return released; });
  });
  {
    std::unique_lock lock(mutex);
    CHECK(wake.wait_for(lock, std::chrono::seconds(2), [&] { return entered; }),
          "the existing worker is actually occupied");
  }
  auto maintenance = std::make_unique<AssetCacheReclaimer>(workers, path.string());
  maintenance->Advance();
  for (int poll = 0; poll < 1000; ++poll) { maintenance->Advance(); }
  CHECK(maintenance->Inspect().Phase == AssetCacheReclaimer::Status::Running &&
            maintenance->Inspect().Steps == 0 && !std::filesystem::exists(path),
        "dispatch and repeated frame polls perform no filesystem work or duplicate steps");
  maintenance.reset();
  CHECK(!std::filesystem::exists(path), "frame destruction neither waits for nor starts cache IO");
  {
    const std::scoped_lock lock(mutex);
    released = true;
  }
  wake.notify_all();
  workers.Wait(blocker);
  const auto drained = workers.Post([] {});
  workers.Wait(drained);
  CHECK(!std::filesystem::exists(path), "a canceled queued operation never opens the database");
  std::error_code error;
  std::filesystem::remove_all(path, error);
  return Report();
}
