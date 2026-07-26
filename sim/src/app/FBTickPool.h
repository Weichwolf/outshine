/* FlightBox — FBTickPool: the lockstep worker pool behind the mission runner's parallel Run() phase.
 * A GYM-ONLY facility by decision (CLAUDE.md "Multi-Unit"): native and wasm stay single-threaded in the
 * sim loop, so this header is included by app/FBMissionRunner.cpp alone and never reaches the browser
 * build (no pthreads/SharedArrayBuffer requirement) or the core library.
 *
 * WHAT IT IS: N-1 worker threads created ONCE for the whole run (at 10 Hz over thousands of ticks a
 * thread per tick would be pure spawn overhead) that sit on a condition variable until RunTick() hands
 * them a job. RunTick(job, count) runs job.RunIndex(0..count-1) exactly once each, spread over the
 * workers PLUS the calling thread, and returns only when every index has finished — that return IS the
 * lockstep barrier. `--threads 1` creates no thread at all and RunTick degenerates to a plain inline
 * loop: the sequential reference path, structurally the same code rather than a second one.
 *
 * WHY IT CANNOT AFFECT A RESULT: which thread takes which index is decided by an atomic counter, i.e.
 * by whoever is free (dynamic scheduling — the point of it is a cast with wildly uneven per-unit cost, a
 * unit rolling on the runway next to one at cruise). That is legitimate only because the indices are
 * INDEPENDENT: a unit's step touches its own airframe and its own module, reads other units only through
 * poses published in the PREVIOUS tick (FBUnit::GetPose's snapshot contract), and everything with a
 * cross-unit or process-wide reach — elevation sampling, pose publication, the monitors, telemetry, the
 * model loading that JSBSim's static unit-conversion map is not safe for — stays outside this call, in
 * the runner's sequential phases. Log output is captured per unit and replayed in unit order at the
 * barrier (app/FBLogSinks.h's FBBufferedLogSink), so not even a line's position depends on scheduling.
 *
 * A PLAIN condition-variable barrier, on measurement rather than on taste: an F-16 unit-step is ~100 us,
 * so the obvious worry is that parking and re-waking the threads 10x per sim-second costs more than it
 * saves. It does not. A bounded-spin variant (spin on an atomic generation/busy counter, park only after
 * ~4000 iterations) was built and measured on the same missions and moved nothing — 1.41x vs 1.41x at
 * two threads, 1.72x vs 1.68x at four, inside the run-to-run spread. The ceiling is the machine, not the
 * barrier: two INDEPENDENT fb-gym processes on this host scale exactly as badly (0.42 s alone -> 0.58 s
 * each when two run side by side = 1.45x aggregate), so the extra complexity bought nothing and is gone. */
#ifndef FBTICKPOOL_H
#define FBTICKPOOL_H
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

namespace FlightBox {

/* One tick's worth of independent per-index work. Implemented by the caller (the runner's actor-step
 * job); RunIndex may be entered from any pool thread, and for a given index exactly once per RunTick. */
class FBTickJob {
public:
  virtual ~FBTickJob() = default;
  virtual void RunIndex(size_t index) = 0;
};

class FBTickPool {
public:
  /* `threads` counts the CALLING thread, so 1 spawns nothing. Values below 1 are read as 1. */
  explicit FBTickPool(size_t threads);
  ~FBTickPool();
  FBTickPool(const FBTickPool &) = delete;
  FBTickPool &operator=(const FBTickPool &) = delete;

  void RunTick(FBTickJob &job, size_t count);
  size_t Threads() const { return Workers_.size() + 1; }

private:
  void WorkerLoop();
  void Drain();   /* take indices off the shared counter until none are left */

  std::vector<std::thread> Workers_;
  std::mutex M_;
  std::condition_variable Start_, Done_;
  FBTickJob *Job_ = nullptr;
  size_t Count_ = 0;
  std::atomic<size_t> Next_{0};   /* the shared index counter = the dynamic schedule */
  size_t Generation_ = 0;   /* bumped per tick — the wake-up predicate, immune to spurious wakes */
  size_t Busy_ = 0;         /* workers still inside this tick; 0 = barrier open */
  bool Stop_ = false;
};

} // namespace FlightBox
#endif /* FBTICKPOOL_H */
