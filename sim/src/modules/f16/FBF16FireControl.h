/* FlightBox — FBF16FireControl: this jet's fire-control computer. Four products, one bus block —
 * the 'B' slant range, the air-to-air launch zone, the air-to-ground delivery solution and the target
 * estimate a launched round is programmed with. Not a generic slot: every convention in it is THIS
 * airframe's. Nothing here queries terrain; a fire-control computer cannot. Herleitung aller Zahlen und
 * Formeln: doc/flightbox/modules-f16.md §8. */
#ifndef FBF16FIRECONTROL_H
#define FBF16FIRECONTROL_H

#include "FBBallistics.h"
#include "FBBfmTrack.h"
#include "FBGun.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"
#include "FBFdm.h"

namespace FlightBox::Modules {

/* Metres and seconds; a time of -1 = "the round dies before it gets there", not zero. */
struct FBLaunchZone {
  bool   Valid = false;
  double RaeroM = 0.0;
  double RtrM = 0.0;
  double RminM = 0.0;
  double TimeToActiveS = -1.0;
  double TimeToImpactS = -1.0;
};

class FBF16FireControl {
public:
  virtual ~FBF16FireControl() = default;

  /* `selected` null or unguided = no launch zone to compute; `gun` null = no gun solution either.
   * `nowS` is absolute sim time, because the target track ages against it. */
  virtual void Run(FBState &state, const Fdm::fb_fdm_state &own, const FBStoreSpec *selected,
                   const FBGunSpec *gun, double nowS, double dt);

  /* What the SMS programs a round with and then radiates as its midcourse uplink; invalid whenever the
   * radar has no lock the tracker could stand on. */
  const FBWeaponTargetState &TargetState() const { return Target_; }

  /* The unguided counterpart, stamped onto a bomb at release so what the computer PREDICTED leaves the
   * aircraft with the weapon. The mode decides nothing about the arithmetic — it is what the release
   * record has to carry, being the statement of which cue the pilot released on. */
  const FBReleaseSolution &ReleaseSolution() const { return Solution_; }
  void SetDeliveryMode(FBDeliveryMode m) { Mode_ = m; }
  FBDeliveryMode DeliveryMode() const { return Mode_; }

  /* A free static: pure arithmetic on a weapon's performance table plus one engagement geometry, so a
   * harness or a future intercept AI can call it without a fire-control computer around it. Model,
   * range formulas and the three deliberate omissions: doc/flightbox/modules-f16.md §8.2. */
  static FBLaunchZone SolveLaunchZone(const FBWeaponPerf &perf, double ownSpeedMs, double altM,
                                      double rangeM, double closureMs, double ownLosMs,
                                      double tgtSpeedMs);

  /* Minimum-turn allowance added to Rmin [SET]: a just-armed round still has to pull its nose onto the
   * target, and a few hundred metres is what that costs at launch speed. */
  static constexpr double kMinTurnM = 300.0;

  /* EEGS funnel limits [DOC] weapons.md §2.5 "Funnel geometry (Level II)", converted once. The span is
   * [MODELL] — the source says it must be configured and does not supply it, so a like-type target is
   * assumed and the pinned f16.xml's own <wingspan> used. */
  static constexpr double kFunnelMinRangeM = 182.88;   /*   600 ft */
  static constexpr double kFunnelMaxRangeM = 914.40;   /* 3,000 ft */
  static constexpr double kTargetSpanM = 9.144;

private:
  /* Split out because each asks a different question against the same box's picture: whether a missile
   * would arrive, where the gun has to point, and where a bomb would land. */
  void SolveGun(FBState &state, const Fdm::fb_fdm_state &own, const FBGunSpec *gun, const FBBfmBlock &trk);
  void SolveGroundAttack(FBState &state, const Fdm::fb_fdm_state &own, const FBStoreSpec *selected);

  Pilot::FBBfmTrack Track_;                  /* this box's own fused picture of the locked contact */
  FBWeaponTargetState Target_{};
  FBReleaseSolution Solution_{};
  FBDeliveryMode Mode_ = FBDeliveryMode::Ccip;
};

} // namespace FlightBox::Modules
#endif
