/* The lockstep worker pool behind the mission runner's STEP phase. GYM-ONLY by decision: included by
 * app/FBMissionRunner.cpp alone, so it never reaches the browser build or the core library.
 *
 * THE BARRIER: RunTick(job, count) runs job.RunIndex(0..count-1) exactly once each over the workers PLUS
 * the calling thread, and returns only when every index has finished — that RETURN is the barrier.
 * `--threads 1` spawns no thread and degenerates to an inline loop: the sequential reference path, the
 * same code rather than a second one.
 *
 * WHY SCHEDULING CANNOT AFFECT A RESULT: which thread takes which index is an atomic counter, i.e.
 * whoever is free. That is legitimate ONLY because the indices are independent — a unit's step touches
 * its own airframe and module and reads others only through poses published in the PREVIOUS tick.
 * Everything with a cross-unit or process-wide reach MUST stay outside this call, in the runner's
 * sequential phases: elevation sampling, pose publication, the monitors, telemetry, and model loading
 * (JSBSim's static unit-conversion map is not safe for it). Log output is captured per unit and replayed
 * in unit order at the barrier, so not even a line's position depends on scheduling.
 *
 * A plain condition-variable barrier ON MEASUREMENT, not taste: a bounded-spin variant was built and
 * moved nothing. Zahlen: doc/flightbox/units-and-missions.md, Abschnitt 9. */
#ifndef FBTICKPOOL_H
#define FBTICKPOOL_H
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

namespace FlightBox::Missions {

/* RunIndex may be entered from ANY pool thread, and for a given index exactly once per RunTick. */
class FBTickJob {
public:
  virtual ~FBTickJob() = default;
  virtual void RunIndex(size_t index) = 0;
};

class FBTickPool {
public:
  /* Counts the CALLING thread, so 1 spawns nothing. */
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

} // namespace FlightBox::Missions
#endif /* FBTICKPOOL_H */
