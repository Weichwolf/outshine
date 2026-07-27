/* FlightBox — FBGunProjectiles: the rounds in the air. A fixed pool of ballistic BUNDLES (core/FBGun.h),
 * owned by the CLIENT that owns the simulation, stepped by it, and read by it to resolve what a burst
 * hit — core/FBDamageModel's structural sibling in every respect that matters:
 *   - a module can neither reach it nor construct one, so no aircraft flies its own bullets and none can
 *     decide what they did (CLAUDE.md "Kein Cheaten"). The gun SYSTEM produces burst records and stops
 *     there, exactly as the SMS produces release records and stops there;
 *   - nothing in it is random, time-dependent or hidden: the same burst from the same geometry flies the
 *     same trajectory, which is what makes a gun engagement reproducible across thread counts;
 *   - it allocates nothing. The pool is a plain array; a bundle that cannot be taken is COUNTED
 *     (Dropped()) rather than silently lost, because a pool that quietly ate a burst would make a
 *     magazine's arithmetic stop adding up.
 *
 * WHY A BUNDLE IS NOT A UNIT (units/FBUnit). A released store becomes a unit because it is one object
 * with its own airframe, its own FDM and its own judgement. A tick of gunfire is ten rounds, from one
 * aircraft, at 0.1 s intervals — a sustained fight would produce thousands of them, each needing a
 * JSBSim instance, a telemetry file and a monitor. What the rounds ARE, physically, does not justify
 * that: they are unguided lumps with no systems and no decisions, and the only question ever asked of
 * them is where they are and what they hit. So they live here, as arithmetic.
 *
 * LIFETIME, and the one thing this class deliberately does not model: a bundle is retired when it has
 * flown kMaxAgeS or kMaxPathM, whichever comes first — both well past the ranges the gun is used at
 * (doc/f16/weapons.md §2.5 puts the funnel's own limit at 3,000 ft). Rounds are NOT tracked to the
 * ground and there is no ballistic impact on terrain: air-to-air gunnery is what this pool is for, and
 * claiming a strafing footprint nothing here computes would be worse than the stated absence. */
#ifndef FBGUNPROJECTILES_H
#define FBGUNPROJECTILES_H

#include "FBGun.h"

namespace FlightBox {

class FBGunProjectiles {
public:
  /* Enough for four aircraft firing continuously for the whole life of a bundle (10 ticks) with room to
   * spare; a squeeze produces one bundle per tick. */
  static constexpr int kMaxBundles = 64;
  static constexpr double kMaxAgeS = 3.0;
  static constexpr double kMaxPathM = 3000.0;

  /* ONE bundle in flight. Both the previous and the current position are carried because a hit is a
   * closest-approach computation over the tick's SEGMENT (a round covers ~100 m per 0.1 s tick, so a
   * per-tick distance test would miss almost everything) — the same reason the proximity fuze works on
   * segments, and the caller does that computation with the same helper. */
  struct Bundle {
    bool   Live = false;
    int    LauncherId = 0;
    const FBGunSpec *Spec = nullptr;
    double Rounds = 0.0;
    double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;
    double PrevLatDeg = 0.0, PrevLonDeg = 0.0, PrevAltM = 0.0;
    double VelE = 0.0, VelN = 0.0, VelU = 0.0;
    double PathM = 0.0;      /* travelled path length — the dispersion pattern's lever arm */
    double AgeS = 0.0;
    double FiredS = 0.0;     /* sim time the burst left the muzzle */
  };

  /* Takes a burst into the pool. False = the pool was full and the burst was dropped (counted). */
  bool Launch(const FBGunBurst &burst);

  /* Integrates every live bundle for `dt` and retires the ones that have run out of life. */
  void Step(double dt);

  /* The caller's verdict: this bundle has been resolved against a target and is spent. A bundle can hit
   * once — the rounds it stood for are gone into the target. */
  void Retire(int index);

  int Capacity() const { return kMaxBundles; }
  const Bundle &At(int i) const { return B_[i]; }
  int LiveCount() const;
  int LaunchedCount() const { return Launched_; }
  int DroppedCount() const { return Dropped_; }

private:
  Bundle B_[kMaxBundles]{};
  int Launched_ = 0, Dropped_ = 0;
};

} // namespace FlightBox
#endif
