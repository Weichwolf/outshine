/* FlightBox — FBMig29FireControl: this aircraft's weapon-control computer. Three products, one bus
 * block — the slant range to the steerpoint, the AIR-TO-AIR launch zone of the selected round, and the
 * target estimate a launched round is programmed with (and then illuminated on).
 *
 * IT IS NOT modules/f16/FBF16FireControl UNDER ANOTHER NAME, and the differences are documented ones:
 *   - the three range numbers are the SAME three physical quantities under this jet's own labels —
 *     Dr max1 / Dr max2 / Dr min map 1:1 onto Raero / Rtr / Rmin (doc/modules/mig29/weapons.md §3.5),
 *     which is why the shared arithmetic (weapons/FBLaunchZone) serves both and no field had to be
 *     added to FBState;
 *   - there is NO CCIP/CCRP solution here, permanently. The 9-12 carries no guided air-to-ground
 *     weapon at all (§1) and its unguided delivery is a DIRECTOR the pilot flies rather than a release
 *     moment he reacts to (§5.3, §5.4) — publishing the Ag* half would be claiming a computation this
 *     jet does not make. What it publishes instead is FBMig29Director's own half of the block, whose
 *     scales appear in the documented order: a range before the trigger, a time after it;
 *   - the gun solution is the SHARED ballistics (core/FBGunBallistics), because a 30 mm projectile is
 *     an unguided lump on a ballistic arc whichever aircraft fired it. What differs is the SIGHT this
 *     jet draws around it, and that is display work this module does not do yet.
 *
 * Nothing here queries terrain; a weapon-control computer cannot. doc/modules/mig29/module.md. */
#ifndef FBMIG29FIRECONTROL_H
#define FBMIG29FIRECONTROL_H

#include "FBBfmTrack.h"
#include "FBGun.h"
#include "FBLaunchZone.h"
#include "FBMig29Director.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"
#include "FBFdm.h"

namespace FlightBox::Modules {

class FBMig29FireControl {
public:
  virtual ~FBMig29FireControl() = default;

  /* `selected` null or unguided = no launch zone; `gun` null = no gun solution either. `nowS` is
   * absolute sim time, because the target track ages against it. */
  virtual void Run(FBState &state, const Fdm::fb_fdm_state &own, const FBStoreSpec *selected,
                   const FBGunSpec *gun, double nowS, double dt);

  /* What the stores system programs a round with — and, for a semi-active round, what its own seeker
   * then lives on for the whole time of flight. */
  const FBWeaponTargetState &TargetState() const { return Target_; }

  /* The air-to-ground branch of this computer. Public because the MODULE routes to it: the LOCKON
   * button and the trigger are cockpit controls, and which box on this panel answers them is the
   * aircraft's knowledge, not the generic layer's. */
  FBMig29Director &Director() { return Director_; }
  const FBMig29Director &Director() const { return Director_; }

  /* [MODEL] The target span the gun sight scales with, the same convention the F-16's sight uses: a
   * like-type target, so this jet's own wingspan (11.36 m, doc/modules/mig29/flight-model-spec.md). */
  static constexpr double kTargetSpanM = 11.36;
  /* [DOC] doc/modules/mig29/weapons.md §4.2: aimed fire 1,220-200 m, EFFECTIVE 790-200 m, and the two
   * independent sources agree to 1 %. The effective band is what the sight declares in range. */
  static constexpr double kGunMinRangeM = 200.0;
  static constexpr double kGunMaxRangeM = 790.0;

private:
  void SolveGun(FBState &state, const Fdm::fb_fdm_state &own, const FBGunSpec *gun, const FBBfmBlock &trk);

  Pilot::FBBfmTrack Track_;   /* this box's own fused picture of the locked contact */
  FBWeaponTargetState Target_{};
  FBMig29Director Director_;
};

} // namespace FlightBox::Modules
#endif
