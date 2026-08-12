/* WHERE A REGION IS GENERATED, and it is not the thread that draws. One region at a time by
 * construction (`doc/architecture.md`, threads: "one job in flight by construction — dedicated,
 * one"): the ring hands over the ground and the space, the thread runs the generators over it, and
 * the caller collects a finished region or nothing.
 *
 * THERE IS NO CANCELLATION TOKEN INSIDE THE GENERATORS. A token would make a region a function of
 * when it was cancelled instead of a function of place, and every determinism the seed buys would
 * go with it. `Cancel` drops the RESULT — the work runs to its end and the lease comes back. */
#ifndef REGIONFORGE_H
#define REGIONFORGE_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "GeneratorSet.h"
#include "Ground.h"
#include "Region.h"
#include "RegionPool.h"
#include "Yield.h"

namespace outshine::Clients {

class RegionForge {
public:
  /* WHAT ONE REGION BECAME: the ground it grew on, the space it claimed and what the generators
   * wanted counted beside the claim. The lease travels with it because the bodies live in the
   * pool's buffers, and the yields name the stretch of them each generator took. */
  struct Grown {
    Generators::Ground Where;
    Generators::RegionPool::Lease Space;
    std::vector<Generators::Yield::Note> Notes;
    std::vector<Generators::Yield> Yields;
    double OccupyMs = 0.0;
  };

  explicit RegionForge(const Generators::GeneratorSet &generators);
  ~RegionForge();
  RegionForge(const RegionForge &) = delete;
  RegionForge &operator=(const RegionForge &) = delete;

  /* Precondition: nothing under way and nothing uncollected. The caller knows both from its own
   * bookkeeping, which is why they are asserted and not queried (`I.6`). */
  void Request(const Generators::Ground &ground, Generators::RegionPool::Lease space);
  /* Which region is being grown or is waiting to be collected, so the ring can ask whether it still
   * names it. None when the forge is idle. */
  std::optional<Generators::Region> UnderWay() const;
  [[nodiscard]] bool Idle() const;
  /* The finished region, moved out. None while one is still growing. */
  std::optional<Grown> Collect();
  /* Drops what is under way: the result is discarded when it arrives and the lease goes back. */
  void Cancel();

private:
  void Run();

  /* Three states, not a boolean over three: an empty request means either "nothing asked" or "the
   * thread has taken it", and those are not the same thing (`ES.9`). */
  enum class Stage { Idle, Growing, Done };

  struct Order {
    Generators::Ground Where;
    Generators::RegionPool::Lease Space;
  };

  const Generators::GeneratorSet &Gens_;

  mutable std::mutex Mu_;
  std::condition_variable Cv_;
  std::optional<Order> Pending_;
  std::optional<Grown> Result_;
  std::optional<Generators::Region> Where_;
  Stage Stage_ = Stage::Idle;
  bool Dropped_ = false;
  bool Stop_ = false;

  std::thread Thread_;
};

} // namespace outshine::Clients
#endif
