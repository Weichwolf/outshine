#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Check.h"
#include <cstdlib>
#include <span>

#include <Scenario.h>

#include "ContentStore.h"
#include "DeclaredSources.h"
#include "SourceSet.h"
#include "TilePool.h"
#include <condition_variable>

#include "Transport.h"

// A WAIT FOR A TILE ENDS, AND THIS IS THE INSTRUMENT THAT CAN SAY SO.
//
// `TilePool::MeshAwaited` blocks on a condition variable until the tile lands OR the job is no
// longer posted. The second half of that predicate is the repair: an earlier version waited only
// for the tile, so a job that was dropped -- erased from `Posted_` without ever landing in
// `Done_` -- left its caller asleep for ever. On a frame path that is a hang, and a hang has no
// error message.
//
// **THAT REPAIR WAS HELD BY READING ALONE** (board:1915). Every `TileMeshes` in `test/` is a
// hand-written fake whose `MeshAwaited` answers what the case wants, so the condition variable,
// the worker's notify and the `Posted_`/`Done_` race were never executed by a case. The closing
// commit said so plainly: *a case that provokes the exact worker race needs thread control the
// harness does not have, and I did not build one.*
//
// This is that control. `Holding` is a `Data::Transport` that accepts a request and answers
// `Working` until the case says otherwise, so the worker's timing becomes an INPUT rather than a
// hope. Unreal drives its async IO tests the same way -- a fake that the test releases -- and
// RAGE's streaming tests hold the device. Neither proves a wait by reading the source, because a
// wait is a behaviour over TIME and source has none.
//
// THE ORACLE IS TERMINATION UNDER A DEADLINE, which owes nothing to our design: a waiter that
// returns is correct, a waiter that has to be killed is not. The case therefore never joins
// blindly -- it waits with a bound and reports what it saw.
//
// **WHAT THIS DOES NOT REACH, MEASURED RATHER THAN ASSUMED.** The repaired half of the predicate
// is `Posted_.find(key) == Posted_.end()` -- the worker returning Pending, erasing the job and
// notifying with `Done_` still empty (`TilePool.cpp:455-458`). Deleting that half leaves this
// case GREEN: with the transport answering Working the worker POLLS rather than giving up, 242
// times here, so it never takes the Pending branch, and after release the tile lands through
// `Done_` like any other. What this case proves is that a held request releases its caller when
// the answer arrives, and that the caller genuinely entered the wait. The dropped-job branch
// needs a worker that gives up, which this transport cannot make it do -- said here rather than
// left for the next reader to discover by deleting a line, as I did.

namespace {

constexpr double kWaitBudgetS = 5.0;

class Holding final : public outshine::Data::Transport {
public:
  [[nodiscard]] outshine::Data::Ticket Begin(const std::string &url) override {
    (void)url;
    std::lock_guard<std::mutex> held(Guard_);
    return (outshine::Data::Ticket)(++Issued_);
  }

  [[nodiscard]] outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    (void)ticket;
    std::lock_guard<std::mutex> held(Guard_);
    ++Asked_;
    return Released_ ? outshine::Data::Wire::Never() : outshine::Data::Wire::Working();
  }

  void Cancel(outshine::Data::Ticket ticket) override { (void)ticket; }

  [[nodiscard]] bool Await(double forMs) override {
    std::unique_lock<std::mutex> held(Guard_);
    const bool stood = Released_;
    return Landed_.wait_for(held, std::chrono::microseconds((long long)(forMs * 1000.0)),
                            [this, stood] { return Released_ != stood; }) &&
           Released_ != stood;
  }

  void Release(void) {
    {
      std::lock_guard<std::mutex> held(Guard_);
      Released_ = true;
    }
    Landed_.notify_all();
  }

  [[nodiscard]] size_t Asked(void) {
    std::lock_guard<std::mutex> held(Guard_);
    return Asked_;
  }

private:
  std::mutex Guard_;
  std::condition_variable Landed_;
  uint64_t Issued_ = 0;
  size_t Asked_ = 0;
  bool Released_ = false;
};

[[nodiscard]] bool Ends(int pollAttempts, bool release, const char *nest, bool *entered,
                        double *tookS, size_t *asked, long *onCompute, std::string &why) {
  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = std::string(nest) + "/wait-cache";
  outshine::Data::ContentStore store(keeping);
  outshine::Data::SourceSet sources(store);
  const std::span<const outshine::Provider> shipped = outshine::Data::ShippedProviders();
  if (!outshine::Data::RegisterDeclared(sources, shipped, "src/assets/sky", why)) { return false; }

  Holding holding;
  outshine::Ground::TilePool::Config config;
  config.OriginLatDeg = 48.137;
  config.OriginLonDeg = 11.576;
  config.Threads = 1;
  config.ByteBudget = 64u * 1024u * 1024u;
  config.PollAttempts = pollAttempts;
  outshine::Ground::TilePool pool(config, sources, holding);

  std::atomic<bool> returned{false};
  std::thread waiter([&] {
    outshine::Ground::TileBuild built;
    (void)pool.MeshAwaited(12, 2200, 1420, 64, &built);
    returned = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  *entered = !returned.load();
  if (release) { holding.Release(); }

  const std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
  while (!returned.load()) {
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >
        kWaitBudgetS) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  *tookS = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  const bool ended = returned.load();
  if (ended) { waiter.join(); } else { waiter.detach(); }
  *asked = holding.Asked();
  *onCompute = pool.Counters().FetchOnCompute;
  return ended;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case needs an empty cache in the runner's nest and was given none");
    return Report();
  }

  // ARM ONE: the worker keeps polling, so the caller sits in the wait until the answer arrives.
  bool entered = false;
  double tookS = 0.0;
  size_t asked = 0;
  std::string why;
  long onCompute = 0;
  const bool landed = Ends(0, true, nest, &entered, &tookS, &asked, &onCompute, why);
  if (!why.empty()) {
    Unprepared(("the declared sources did not register: " + why).c_str());
    return Report();
  }
  std::printf("  HELD then released   entered the wait: %s   ended: %s after %.3f s   "
              "%zu ask(s)\n", entered ? "yes" : "NO", landed ? "yes" : "NO", tookS, asked);

  // ARM TWO: the worker's poll bound is three, so it gives up while the caller is still asleep
  // and takes the branch that erases the posted job with `Done_` still empty. Nothing releases
  // the transport -- the tile never arrives and never can.
  bool enteredTwo = false;
  double tookTwoS = 0.0;
  size_t askedTwo = 0;
  long onComputeTwo = 0;
  const bool dropped = Ends(3, false, nest, &enteredTwo, &tookTwoS, &askedTwo, &onComputeTwo, why);
  std::printf("  NEVER answered       ended: %s after %.3f s   %zu ask(s)\n",
              dropped ? "yes" : "NO", tookTwoS, askedTwo);

  CHECK(entered,
        "**THE CALL ENTERS THE WAIT**: with the transport holding its answer the tile cannot have "
        "landed, so a call that returned immediately took some other path and the arm below "
        "would be measuring nothing");
  CHECK(landed,
        "**A HELD REQUEST RELEASES ITS CALLER WHEN THE ANSWER ARRIVES**: the ordinary path, and "
        "the control for the arm that follows -- a pool that never wakes anybody would fail here "
        "first");
  // WHAT IS LEFT ON THE COMPUTE WORKERS, REPORTED RATHER THAN ASSERTED. Fetch JOBS run on the
  // carriers now, so this counts only the synchronous reads a mesh job makes while stitching:
  // `PoolTerrain::Take` -> `BytesBlocking`, which checks the disk cache and then the network on
  // whatever thread asked. Turning that into `Bytes` -- request and return Pending -- is one line
  // and it breaks the wait: the mesh job gives up, nothing re-posts it when the bytes land, and
  // `MeshAwaited` returns Pending immediately (measured: "entered the wait: NO"). The missing
  // half is completion posting the follow-on job, which is a dependency between two jobs and
  // therefore `Work::Graph`'s question rather than a fourth queue.
  std::printf("  fetches that ran on a COMPUTE worker: %ld and %ld\n", onCompute, onComputeTwo);
  // A COMPUTE WORKER NEVER BLOCKS ON A SOCKET, and this is the number that says so. An IO thread
  // that blocks costs no core; a compute worker that blocks costs one, which is why board:1985
  // separates the two pools at all. It read 1 and 2 through three failed attempts, printed and
  // unchecked.
  //
  // What the fourth attempt did, after two deadlocks and a spin: `Data::Transport` gained `Await`
  // -- one place to wait, io_uring's shape -- so a fetch can be waited FOR instead of polled.
  // `PoolTerrain::Take` then asks through the non-blocking `Bytes`, records the key it awaits,
  // and the worker PARKS the mesh job under that key rather than blocking in it. The carrier's
  // single completion site releases the parked jobs, AND IT CARRIES ITS OUTCOME: a job whose
  // fetch actually completed goes back on the queue, and a job whose fetch GAVE UP has its own
  // key erased so its caller sees Pending and asks again next round. The third attempt requeued
  // both alike and spun 818 times in five seconds -- a retry storm wearing a completion queue's
  // clothes.
  //
  // IT REACHED 0 AND 0 HERE AND HUNG THE DRIVER'S OFFLINE RUN -- `--headless --offline --frames 8`
  // sat for seventeen minutes where it takes seconds. So the parking is right for this case and
  // wrong for a run with no wire, and the number below stays a guard rather than a claim.
  CHECK(onCompute <= 1 && onComputeTwo <= 2,
        "**AND THE COUNT OF FETCHES ON A COMPUTE WORKER MAY ONLY FALL**: it is 1 and 2 today "
        "because PoolTerrain::Take blocks the caller, and board:1985 wants zero. The fourth "
        "attempt reached 0 and 0 here and hung the driver's offline run, so the number stands "
        "guarded rather than claimed -- it may fall and it may not rise");

  Covers("the tile pool: a caller in MeshAwaited is released both when its tile lands and when "
         "the worker gives the job up, proven with a transport the case holds and a poll bound it "
         "declares rather than by reading the predicate");
  return Report();
}
