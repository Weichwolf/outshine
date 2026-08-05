/* FlightBox — FBAirFireControl: the CATALOGUE ROW'S fire control, and it is deliberately the smallest
 * box with which a declared weapon can leave the aeroplane.
 *
 * WHY IT EXISTS. Until 2026-07-29 FBAirModule composed none, so FBState::FireControl was never written
 * for any of the eighteen rows and all three of pilot/FBPilot's employment gates (the intercept shot's
 * `inParams`, BfmMissileShot, BfmGunfire) stayed shut on every one of them. Measured: an `f15c` with
 * four AIM-120 designated at 18.64 nm, held its lock for 28 s down to 8.8 nm, and never pressed.
 *
 * WHAT IT IS NOT. modules/f16/FBF16FireControl is the REFERENCE and not the template: this box drops
 * the air-to-ground half whole (CCIP/CCRP integration, the release solution, the delivery mode) and the
 * steerpoint ranging line, because no catalogue tier accepts `attack`, no catalogue radar can see the
 * ground (module.md A7) and no catalogue cell has a display to range for. Three products remain, and
 * each of them has a named reader:
 *
 *   1. THE LAUNCH ZONE for the round on the selected rail — weapons/FBLaunchZone, the same arithmetic
 *      every fire control in the tree runs. Read by FBStoresSystem's release interlock and by the
 *      pilot's shot gate.
 *   2. THE TARGET ESTIMATE a launched round is programmed with, and through it the SHOOTER'S BINDING:
 *      FBStoresSystem stops radiating the midcourse uplink the moment this estimate goes invalid, so a
 *      semi-active round loses its guidance the moment its shooter breaks the lock. Nothing new had to
 *      be built for that; it had to be CONNECTED.
 *   3. THE GUN SOLUTION, off the same track, for the ten rows with a cannon.
 *
 * WHAT THE ROW ITSELF DECIDES is what the catalogue publishes and nothing else: whether it has a radar
 * to lock with, which round is on the rail (and therefore whether it binds), and its own span (the
 * like-type target the funnel is drawn for — the identical assumption the F-16's EEGS makes, made per
 * row from a number every row publishes). doc/modules/air/module.md §Spec 12. */
#ifndef FBAIRFIRECONTROL_H
#define FBAIRFIRECONTROL_H

#include "FBAircraft.h"
#include "FBBfmTrack.h"
#include "FBFdm.h"
#include "FBGun.h"
#include "FBLaunchZone.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"

namespace FlightBox::Modules {

class FBAirFireControl {
public:
  /* THE FUNNEL AS A TIME-OF-FLIGHT WINDOW, and that is what makes one funnel serve six guns. The F-16's
   * EEGS window is published in DISTANCE (600 / 3 000 ft) for ONE gun; divided by that gun's own muzzle
   * velocity it is 0.178 s / 0.888 s of flight [DERIVED, kM61A1 1 030 m/s], and a gun's usable window is
   * a time of flight rather than a distance — the target's evasion during the round's flight is what
   * ends it. Cross-check on an independent source: 0.888 s at the GSh-301's 860 m/s gives 764 m against
   * that gun's documented 800 m effective air-target limit (doc/modules/mig29/weapons.md §8.4).
   * IT STAYS A NUMBER OF THIS CLASS. Declaring it per gun would write the same two seconds into every
   * row — the argument above IS that one time-of-flight window serves them all, and a copy per barrel
   * would unsay it. The only published origin is ONE sight; the cross-check on a second gun is the line
   * above. verify-types `value`. */
  static constexpr double kFunnelNearS = 0.178;
  static constexpr double kFunnelFarS = 0.888;

  void Configure(const FBAircraftSpec &spec) { Spec_ = &spec; }

  /* `selected` null or unguided = no launch zone; `gun` null = no gun solution. `nowS` is absolute sim
   * time, because the target estimate ages against it. */
  void Run(FBState &state, const Fdm::fb_fdm_state &own, const FBStoreSpec *selected,
           const FBGunSpec *gun, double nowS);

  /* What the SMS programs a round with and then radiates as its midcourse uplink; invalid whenever the
   * radar has no lock the estimate could stand on — which IS the binding. */
  const FBWeaponTargetState &TargetState() const { return Target_; }

private:
  void SolveZone(FBState &state, const Fdm::fb_fdm_state &own, const FBStoreSpec *selected);
  void SolveGun(FBState &state, const Fdm::fb_fdm_state &own, const FBGunSpec *gun);

  const FBAircraftSpec *Spec_ = nullptr;
  Pilot::FBBfmTrack Track_;   /* this box's own estimate of the locked contact, as the F-16's is */
  FBWeaponTargetState Target_{};
};

} // namespace FlightBox::Modules
#endif
