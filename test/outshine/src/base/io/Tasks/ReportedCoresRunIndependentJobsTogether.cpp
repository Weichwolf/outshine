#include "Check.h"
#include "Tasks.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto reported = std::max(1U, std::thread::hardware_concurrency());
  Tasks pool(Tasks::ComputeThreads());
  CHECK(pool.Threads() == static_cast<int>(reported),
        "preparation uses every reported core without a fixed reserve or cap");
  std::mutex mutex;
  std::condition_variable changed;
  unsigned started = 0;
  bool released = false;
  std::vector<Tasks::Handle> jobs;
  jobs.reserve(reported);
  for (unsigned at = 0; at < reported; ++at) {
    jobs.push_back(pool.Post([&] {
      std::unique_lock lock(mutex);
      ++started;
      changed.notify_all();
      changed.wait(lock, [&] { return released; });
    }));
  }
  {
    std::unique_lock lock(mutex);
    CHECK(changed.wait_for(lock, std::chrono::seconds(5), [&] { return started == reported; }),
          "independent preparation jobs can all enter before another job finishes");
    released = true;
  }
  changed.notify_all();
  for (const auto job : jobs) { pool.Wait(job); }
  CHECK(started == reported, "all reported-core jobs complete exactly once");
  return Report();
}
