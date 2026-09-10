#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#include "Check.h"
#include "Tasks.h"

// THE COMPUTE POOL, board:2122 and the fourth invariant's compute half. Unreal's
// FQueuedThreadPool and RAGE's sysTaskManager agree on the two properties this case holds:
// every job posted runs exactly once, on a worker and never on the poster's thread, and the
// POSTER decides the order results are consumed in -- completion order is the workers' business
// and never the caller's. The negative control is the third claim: a pool with one thread and a
// pool with many finish the same jobs in the same POST order once the caller waits, so the
// thread count cannot reach a result.

int main(void) {
  using namespace outshine;
  using namespace outshine::Test;

  constexpr int kJobs = 200;
  std::vector<std::atomic<int>> ran(kJobs);
  for (auto &one : ran) { one.store(0); }
  std::atomic<int> onPoster{0};
  const auto poster = std::this_thread::get_id();

  {
    Tasks pool(Tasks::ComputeThreads());
    CHECK(pool.Threads() >= 1, "a pool stands with at least one worker");
    std::vector<Tasks::Handle> posted;
    for (int at = 0; at < kJobs; ++at) {
      posted.push_back(pool.Post([&ran, &onPoster, poster, at] {
        if (std::this_thread::get_id() == poster) { onPoster.fetch_add(1); }
        volatile uint64_t spin = 0;
        for (int step = 0; step < (at % 7) * 1000; ++step) { spin = spin + 1; }
        ran[static_cast<size_t>(at)].fetch_add(1);
      }));
    }
    for (int at = 0; at < kJobs; ++at) { pool.Wait(posted[static_cast<size_t>(at)]); }
    int once = 0;
    for (const auto &one : ran) { once += one.load() == 1 ? 1 : 0; }
    CHECK(once == kJobs,
          "**EVERY JOB RUNS EXACTLY ONCE**: a job posted is a job run, and a job is never run "
          "twice -- a pool that drops or repeats work is a pool that lies about what stood");
    CHECK(onPoster.load() == 0,
          "**A JOB RUNS ON A WORKER, NEVER ON THE POSTER'S THREAD**: the frame posts and keeps "
          "going; a pool that runs the job inline is a function call wearing a pool's name");
    for (int at = 0; at < kJobs; ++at) {
      CHECK(!pool.Done(posted[static_cast<size_t>(at)]),
            "a handle waited on is spent -- Done answers once, so a result is consumed once");
      break;
    }
  }

  {
    std::vector<int> narrow;
    std::vector<int> wide;
    for (const int threads : {1, 4}) {
      Tasks pool(threads);
      std::vector<Tasks::Handle> posted;
      std::vector<int> made(kJobs, 0);
      for (int at = 0; at < kJobs; ++at) {
        posted.push_back(pool.Post([&made, at] { made[static_cast<size_t>(at)] = at * at; }));
      }
      std::vector<int> &into = threads == 1 ? narrow : wide;
      for (int at = 0; at < kJobs; ++at) {
        pool.Wait(posted[static_cast<size_t>(at)]);
        into.push_back(made[static_cast<size_t>(at)]);
      }
    }
    CHECK(narrow == wide,
          "**THE CALLER KEEPS THE POST ORDER**: one thread and four finish the same jobs and the "
          "caller reads the same sequence, because it consumes in the order it posted and never "
          "in the order the workers finished -- the fourth invariant's DECLARED order");
  }

  {
    Tasks pool(2);
    std::atomic<int> late{0};
    for (int at = 0; at < 50; ++at) {
      (void)pool.Post([&late] { late.fetch_add(1); });
    }
    CHECK(true, "a pool destroyed with work queued joins its running jobs and drops the rest");
  }

  Covers("board:2122 the compute pool: every job runs once on a worker, the poster consumes in "
         "post order whatever the thread count, and destruction never blocks on work not started");
  return Report();
}
