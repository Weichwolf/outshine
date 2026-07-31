/* FlightBox — FBOrdnance: everything a released store and a fired burst DO once they have left the
 * jet. It is the OWNER's book and deliberately not a system's: a weapon that scored itself on its own
 * seeker estimate would be the purest form of cheating, so geometry -> damage is resolved out here, on
 * the published poses, by the code that owns the simulation.
 *
 * It exists as a class because there is more than one owner. The headless runner and the browser frame
 * loop both have to drain FBStoresSystem::TakeRelease/FBGunSystem::TakeBurst, fly what came out and
 * resolve what it hit — and two copies of that would be two simulators. Three calls per tick, in this
 * order, because the order is the semantics: Resolve (fly and resolve what was ALREADY in the air) ->
 * Launch (this tick's releases become units) -> SnapPoses (the segment the next CPA is measured over).
 * A bundle is therefore never resolved in the tick it was created in.
 * doc/weapons.md, doc/units-and-missions.md §8. */
#ifndef FBORDNANCE_H
#define FBORDNANCE_H

#include <functional>
#include <memory>
#include <vector>
#include "FBGunProjectiles.h"
#include "FBModelRoots.h"
#include "FBSimUnit.h"
#include "FBStore.h"

namespace FlightBox::Units { class FBUnitRegistry; }

namespace FlightBox::Missions {

class FBOrdnance {
public:
  /* What the OWNER does with a store the moment it became a unit and before it is first stepped: the
   * runner opens a telemetry CSV for it, the browser has no file system and does nothing. Called with
   * the index the store is about to occupy in the actor list. */
  using FBStoreSpawned = std::function<void(Units::FBSimUnit &store, size_t index)>;

  explicit FBOrdnance(const FBModelRoots &models) : Models_(models) {}

  /* Sized ONCE from the ceiling the caller computed (declared actors + every loaded station), so no
   * buffer a tick is holding a reference into is ever resized. */
  void Reserve(size_t maxActors);
  void OnStoreSpawned(FBStoreSpawned fn) { OnSpawn_ = std::move(fn); }

  /* Fly the rounds in the air and resolve what they reached: gun bundles against aircraft, a store's
   * proximity fuze, its impact and the surface burst that follows. */
  void Resolve(Units::FBActorList &actors, double simT, double dt);
  /* THE ACTOR LIST'S ONE GROWTH POINT: drained in actor order, each module's queue FIFO, so ids and
   * tick order are identical no matter how many threads stepped the tick. */
  void Launch(Units::FBActorList &actors, Units::FBUnitRegistry &units, double simT);
  /* The pose snapshot the next tick's closest-approach is measured against. */
  void SnapPoses(const Units::FBActorList &actors);

private:
  /* A released store, from separation to impact. */
  struct FBStoreTrack {
    size_t Index = 0;         /* into the actor list */
    double SpawnS = 0.0;
    double DeadlineS = 0.0;   /* SpawnS + the store's own MaxFlightS (core/FBStore.h) */
    const FBStoreSpec *Spec = nullptr;
    int    LauncherId = 0;
    /* What the computer said would happen, carried out of the jet so the impact can be reported beside it. */
    FBReleaseSolution Solution;
    /* Closest approach to any aircraft OTHER than the launcher — one separating from a pylon passes its
     * own carrier at tens of metres, which is geometry and not aim. */
    double MinMissM = 1e18;
    int    MinMissUnit = 0;
  };

  /* One record per bundle slot: the pool flies the rounds and knows nothing about units, so how close
   * each came and to whom is the OWNER's book, emitted when the bundle's life ends. */
  struct FBGunPass {
    bool   Live = false;
    double MinMissM = 1e18;
    double SpreadM = 0.0, PathM = 0.0, ClosureMs = 0.0, Rounds = 0.0;
    std::string TargetName;
  };

  void ResolveGuns(Units::FBActorList &actors, double dt);
  void ResolveStores(Units::FBActorList &actors, double simT, double dt);
  static void LogStoreImpact(const Units::FBSimUnit &store, const FBStoreTrack &track, double simT,
                             const Fdm::fb_fdm_state &cross, double backS);

  const FBModelRoots &Models_;
  FBStoreSpawned OnSpawn_;
  FBGunProjectiles Bullets_;   /* fixed pool, no tick-path allocation */
  std::vector<FBGunPass> Passes_{(size_t)FBGunProjectiles::kMaxBundles};
  std::vector<FBStoreTrack> Tracks_;
  std::vector<Units::FBUnitPose> PrevPose_;
  bool HavePrevPose_ = false;
};

} // namespace FlightBox::Missions
#endif
