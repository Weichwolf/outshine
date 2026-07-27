/* FlightBox — FBF16FireControl: the F-16's fire-control computer. Three products, one box, one bus
 * block (core/FBAvionicsBlocks.h's FBFireControlBlock).
 *
 * (1) THE 'B' RANGE PROVIDER — slant range to the active steerpoint from horizontal distance + the
 * altitude difference to the STEERPOINT'S OWN elevation (baro method), not FCR ranging. Matches the
 * DCS/real-jet HUD convention (doc/f16/hud-symbology.md: "B: Range computed using steerpoint elevation/
 * barometric elevation") and the FlightGear F-16 mod's `steerpoints.getCurrentSlantRange()` (same
 * Pythagorean horizontal-distance + altitude-diff method, cross-checked against its GPL-2.0 Nasal
 * source for the FORMULA only — no code copied). Unchanged by the DLZ work below.
 *
 * (2) THE AIR-TO-AIR LAUNCH ZONE (the DLZ, doc/f16/weapons.md §2.5) for whatever the SMS has selected.
 * This is what a shot decision is made on, so it is COMPUTED rather than tabled: Raero, Rtr, Rmin and
 * the two countdowns come out of a forward integration of the round's own energy — thrust over the
 * burn, mass falling as the propellant leaves, drag against the air at this altitude — against the
 * geometry the radar is measuring right now. What it integrates is the FCC's stored performance table
 * (core/FBStore.h's FBWeaponPerf), deliberately a coarse copy of what the missile's own JSBSim model
 * does: a fire-control computer works from a table, and the error between its prediction and the flown
 * result is a real property of every shot. The intercept mission measures exactly that error.
 *
 * (3) THE TARGET ESTIMATE the round is programmed with and then supported by. A radar contact is an
 * echo with no velocity (core/FBRadarContact.h), and both the launch zone and the missile's midcourse
 * need to know where the target is GOING — so this box carries its own systems/FBBfmTrack, fed from the
 * LOCKED contact only, and publishes the result as an FBWeaponTargetState (core/FBWeaponUplink.h). The
 * SMS copies it onto the round at launch and radiates it as the midcourse uplink for as long as the
 * lock holds. The pilot's own BFM tracker is a SEPARATE instance for a separate consumer (its pursuit
 * steering) and is untouched by this — one tracker per consumer, no shared mutable picture.
 *
 * F-16-specific (not a generic systems/ slot): the 'B' provider letter, this exact slant-range method
 * and which weapons this jet's FCC has tables for are this airframe's conventions. READS the nav,
 * platform and radar blocks, WRITES the fire-control block — a documented fusion, which is why it
 * checks the heads: with a slant-range source invalid the whole block goes invalid as it always did,
 * and with no radar lock the block still publishes while DlzValid stays false ("no launch solution" is
 * an answer, not an absence). */
#ifndef FBF16FIRECONTROL_H
#define FBF16FIRECONTROL_H

#include "FBBfmTrack.h"
#include "FBGun.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"
#include "FBFdm.h"

namespace FlightBox {

/* What one launch-zone integration produced. Metres and seconds; a time of -1 = "the round dies before
 * it gets there", which is a different statement from zero. */
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

  /* `selected` is the store the SMS currently has selected (null, or an unguided one, means there is no
   * launch zone to compute); `gun` is the gun this aircraft has fitted (null = none, and then there is
   * no gun solution either). `own` is this aircraft's own state and `nowS` the module's sim clock —
   * absolute, because the target track ages against it. */
  virtual void Run(FBState &state, const fb_fdm_state &own, const FBStoreSpec *selected,
                   const FBGunSpec *gun, double nowS, double dt);

  /* The target estimate this box holds, for the SMS to program a round with and to radiate as the
   * midcourse uplink. Invalid whenever the radar has no lock the tracker could stand on. */
  const FBWeaponTargetState &TargetState() const { return Target_; }

  /* THE LAUNCH-ZONE INTEGRATION, a free static because it is pure arithmetic on a weapon's performance
   * table plus one engagement geometry — no member state, so a harness or a future intercept AI can
   * call it without a fire-control computer around it.
   *
   *   ownSpeedMs  the round's launch speed: it leaves the rail with the launcher's velocity
   *   altM        launch altitude; the air density the whole integration runs against (level-flight
   *               approximation — an engagement's altitude band is small next to its range)
   *   rangeM      current slant range to the locked target
   *   closureMs   current closure (+ = closing), as the radar measures it
   *   ownLosMs    the launcher's own speed component along the line of sight
   *   tgtSpeedMs  the target's estimated speed, for the turn-and-run case
   *
   * The model: dv/dt = (T(t) - 0.5*rho*v^2*CA*S)/m(t), T stepping boost -> sustain -> zero, m falling
   * linearly across the burn. The round is DEAD once v drops below MinSpeedMs — past that it can no
   * longer run an intercept, and that is what bounds every range below. Out of ONE integration:
   *   S_m, T_death   how far the round gets and how long that takes
   *   Raero = S_m + Vt_los*T_death    the target keeps closing at its current LOS component
   *   Rtr   = S_m - tgtSpeed*T_death  the target reverses at launch and runs (weapons.md §2.5's Rtr)
   *   Rmin  = closure*ArmingS + kMinTurnM   it must separate, light, arm and still pull onto the target
   *   TTA/TTI   when the range closes to the seeker's activation range, and to zero
   * Deliberately NOT modelled: the lofted midcourse a real AMRAAM flies (worth a good fraction of the
   * range at altitude), the target's own altitude change, and the energy cost of the terminal turn. All
   * three would move Raero in ways nothing available here can source, and a launch zone that claims
   * precision it has not got is worse than one whose simplifications are written down. */
  static FBLaunchZone SolveLaunchZone(const FBWeaponPerf &perf, double ownSpeedMs, double altM,
                                      double rangeM, double closureMs, double ownLosMs,
                                      double tgtSpeedMs);

  /* Minimum-turn allowance added to Rmin [SET]: a round that has only just armed still has to pull its
   * nose onto the target, and a few hundred metres is what that costs at launch speed. */
  static constexpr double kMinTurnM = 300.0;

  /* ---- THE EEGS FUNNEL'S OWN CONSTANTS (doc/f16/weapons.md §2.5, "Funnel geometry (Level II)") ----
   * The funnel is drawn between a MINIMUM and a MAXIMUM range and its width at any point is the
   * target's KNOWN WINGSPAN at the range the gun is currently aimed for; a target that appears smaller
   * than the funnel's wide end is out of range. Both range limits are the guide's own figures, quoted
   * exactly (600 ft / 3,000 ft) and converted once here.
   *
   * The WINGSPAN is the one number the guide says has to be configured and does not supply ("Requires
   * the target's actual wingspan to be configured for correct scaling"). This fire control assumes a
   * like-type target and uses the pinned f16.xml's own <wingspan> (30 ft = 9.144 m), which is both a
   * real number out of the model and the correct one for every engagement this simulator can currently
   * fly (F-16 against F-16). A future FCC that knows what it is looking at replaces the constant with
   * the identified type's span and nothing else about the geometry changes. */
  static constexpr double kFunnelMinRangeM = 182.88;   /*   600 ft */
  static constexpr double kFunnelMaxRangeM = 914.40;   /* 3,000 ft */
  static constexpr double kTargetSpanM = 9.144;

private:
  /* The EEGS half of Run(), split out because it is a different question against the same target
   * estimate: the launch zone asks whether a missile would arrive, the funnel asks where the gun has to
   * point. `trk` is this box's own fused picture, already updated this tick. */
  void SolveGun(FBState &state, const fb_fdm_state &own, const FBGunSpec *gun, const FBBfmBlock &trk);

  FBBfmTrack Track_;                  /* this box's own fused picture of the locked contact */
  FBWeaponTargetState Target_{};
};

} // namespace FlightBox
#endif
