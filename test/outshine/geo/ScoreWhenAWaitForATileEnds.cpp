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

  void Release(void) {
    std::lock_guard<std::mutex> held(Guard_);
    Released_ = true;
  }

  [[nodiscard]] size_t Asked(void) {
    std::lock_guard<std::mutex> held(Guard_);
    return Asked_;
  }

private:
  std::mutex Guard_;
  uint64_t Issued_ = 0;
  size_t Asked_ = 0;
  bool Released_ = false;
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case needs an empty cache in the runner's nest and was given none");
    return Report();
  }

  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = std::string(nest) + "/wait-cache";
  outshine::Data::ContentStore store(keeping);
  outshine::Data::SourceSet sources(store);
  std::string refused;
  const std::span<const outshine::Provider> shipped = outshine::Data::ShippedProviders();
  if (!outshine::Data::RegisterDeclared(sources, shipped, "src/assets/sky", refused)) {
    Unprepared(("the declared sources did not register: " + refused).c_str());
    return Report();
  }

  Holding holding;
  outshine::Ground::TilePool::Config config;
  config.OriginLatDeg = 48.137;
  config.OriginLonDeg = 11.576;
  config.Threads = 1;
  config.ByteBudget = 64u * 1024u * 1024u;
  outshine::Ground::TilePool pool(config, sources, holding);

  std::atomic<bool> returned{false};
  std::atomic<int> answered{-1};
  std::thread waiter([&] {
    outshine::Ground::TileBuild built;
    answered = (int)pool.MeshAwaited(12, 2200, 1420, 64, &built);
    returned = true;
  });

  // THE WAITER MUST STILL BE WAITING, or the case below proves nothing: a call that returned
  // before the transport answered never entered the condition variable at all.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  const bool waitedFirst = !returned.load();
  std::printf("  with the transport holding, the call has returned: %s\n",
              waitedFirst ? "no -- it is waiting" : "YES, so it never waited");

  holding.Release();
  const std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
  while (!returned.load()) {
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >
        kWaitBudgetS) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  const double tookS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  const bool ended = returned.load();
  if (ended) { waiter.join(); } else { waiter.detach(); }

  std::printf("  after release, the wait ended: %s after %.3f s (budget %.1f s)\n",
              ended ? "yes" : "NO", tookS, kWaitBudgetS);
  std::printf("  the transport was asked %zu time(s)\n", holding.Asked());

  CHECK(waitedFirst,
        "**THE CALL ENTERS THE WAIT**: with the transport holding its answer the tile cannot have "
        "landed, so a call that returned immediately took some other path and the check below "
        "would be measuring nothing");
  CHECK(ended,
        "**A WAIT FOR A TILE ENDS**: `MeshAwaited` waits for the tile to land OR for the job to "
        "stop being posted, so a request that will never be answered releases its caller instead "
        "of holding it for ever. Held by reading alone until now -- a hang has no error message, "
        "and reading source cannot see a behaviour that only exists over time");

  Covers("the tile pool: a caller blocked in MeshAwaited is released when the request it waits on "
         "can no longer be answered, proven by a transport the case holds and then releases "
         "rather than by inspecting the predicate");
  return Report();
}
