/* FlightBox — the AIR-TO-AIR LAUNCH ZONE: a forward integration of a round's stored performance table
 * against one engagement geometry. Pure arithmetic on values — no state, no allocation, no bus.
 *
 * WHY IT LIVES IN weapons/ AND NOT IN A MODULE. It is the same computation for every fire-control
 * computer in the tree (the F-16's Raero/Rtr/Rmin and the MiG-29's Dr max1 / Dr max2 / Dr min are the
 * same three numbers under two names, doc/modules/mig29/weapons.md §3.5), and a second airframe
 * copying it would be two implementations of one physical question. What stays with each MODULE is the
 * CONVENTIONS it feeds in — which round is selected, which contact is the target, which plane it
 * ranges against.
 *
 * IT IS DELIBERATELY COARSER THAN THE ROUND. FBWeaponPerf is a stored table; the weapon itself flies a
 * full JSBSim airframe with its own autopilot. The difference between what the computer predicted and
 * what the round did is a REAL property of every shot, which is why the two are not the same code —
 * and why the intercept missions measure it. doc/weapons.md §10.2. */
#ifndef FBLAUNCHZONE_H
#define FBLAUNCHZONE_H

#include "FBStore.h"

namespace FlightBox::Weapons {

/* Metres and seconds; a time of -1 = "the round dies before it gets there", not zero. */
struct FBLaunchZone {
  bool   Valid = false;
  double RaeroM = 0.0;
  double RtrM = 0.0;
  double RminM = 0.0;
  /* How long until the SHOOTER is free — the handover, not the seeker's power-up: 0 for a fire-and-
   * forget round, the time to the activation ring for an active one, and -1 for a SEMI-ACTIVE one,
   * which never lets go at all (core/FBStore.h's FBSeekerHandoverS). */
  double TimeToActiveS = -1.0;
  double TimeToImpactS = -1.0;
};

/* Minimum-turn allowance added to Rmin [SET]: a just-armed round still has to pull its nose onto the
 * target, and a few hundred metres is what that costs at launch speed. */
constexpr double kLaunchZoneMinTurnM = 300.0;

FBLaunchZone FBSolveLaunchZone(const FBWeaponPerf &perf, FBSeekerKind seeker, double ownSpeedMs,
                               double altM, double rangeM, double closureMs, double ownLosMs,
                               double tgtSpeedMs);

} // namespace FlightBox::Weapons
#endif
