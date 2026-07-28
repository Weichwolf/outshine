/* FlightBox — FBF16FireControl: this jet's fire-control computer. Four products, one bus block —
 * the 'B' slant range, the air-to-air launch zone, the air-to-ground delivery solution and the target
 * estimate a launched round is programmed with. Not a generic slot: every convention in it is THIS
 * airframe's. Nothing here queries terrain; a fire-control computer cannot. Herleitung aller Zahlen und
 * Formeln: doc/modules-f16.md §8. */
#ifndef FBF16FIRECONTROL_H
#define FBF16FIRECONTROL_H

#include "FBBallistics.h"
#include "FBBfmTrack.h"
#include "FBGun.h"
#include "FBLaunchZone.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"
#include "FBFdm.h"

namespace FlightBox::Modules {

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

  /* The launch-zone arithmetic itself lives in weapons/FBLaunchZone — it is the same physical question
   * for every fire control in the tree, and the second airframe's Dr max1/max2/min are these three
   * numbers under other names. What stays HERE is which round, which contact and which plane. */

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
