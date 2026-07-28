/* The rounds in the air: a fixed pool of ballistic BUNDLES owned, stepped and resolved by the CLIENT —
 * a module can neither reach it nor construct one, so no aircraft flies its own bullets. Nothing random,
 * time-dependent or hidden; nothing allocates, and a bundle that cannot be taken is COUNTED rather than
 * silently lost. A bundle is deliberately NOT a units/FBUnit (thousands of unguided lumps with no
 * systems and no decisions), and rounds are NOT tracked to the ground: this pool is for air-to-air
 * gunnery, and a strafing footprint nothing here computes would be worse than the stated absence.
 * doc/core.md, Abschnitt 7.6. */
#ifndef FBGUNPROJECTILES_H
#define FBGUNPROJECTILES_H

#include "FBGun.h"

namespace FlightBox {

class FBGunProjectiles {
public:
  /* Enough for four aircraft firing continuously for a bundle's whole life, with room to spare. */
  static constexpr int kMaxBundles = 64;
  static constexpr double kMaxAgeS = 3.0;
  static constexpr double kMaxPathM = 3000.0;

  /* Previous AND current position, because a hit is a closest-approach over the tick's SEGMENT: a round
   * covers ~100 m per tick, so a per-tick distance test would miss almost everything. */
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

  /* The caller's verdict: resolved against a target and spent. A bundle can hit once. */
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
