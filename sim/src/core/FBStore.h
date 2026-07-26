/* FlightBox — the STORE catalogue: what an external store IS, as data. One entry per carriable item,
 * and every number in it is either the store's OWN pinned JSBSim model (vendor/jsbsim/aircraft/<model>,
 * read-only per CLAUDE.md Prinzip 1) or derived from it by a stated formula — nothing about a weapon is
 * invented here, because a released store flies as its own FDM instance of exactly that model and the
 * carriage figures must describe the same object.
 *
 * WHY A CATALOGUE AND NOT A CLASS. A store has no behaviour while it hangs on a pylon: it is mass,
 * drag and a model name. Its behaviour IS the JSBSim model it becomes the moment it is released
 * (modules/stores/FBStoreModule + fdm/FBFdm), so the thing that stays behind on the aircraft is a value
 * type. Lives in core/ for the same reason FBRunway/FBSpawn do: the mission parser, the module's SMS
 * and the app-side spawn path all name it, and none of them may include the others. */
#ifndef FBSTORE_H
#define FBSTORE_H

#include <cstdint>
#include <cstring>
#include "FBWeaponUplink.h"

namespace FlightBox {

/* Append only — the ordinal is telemetry-visible (the per-station store column). None is the empty
 * station and must stay 0 so a zeroed block reads as "nothing loaded". */
enum class FBStoreKind : uint8_t { None = 0, Mk82, Aim120 };

/* THE FIRE-CONTROL COMPUTER'S OWN PERFORMANCE TABLE for a guided round — the coarse model a launch-zone
 * computation runs on (modules/f16/FBF16FireControl). It is DELIBERATELY a separate, simplified copy of
 * what the weapon's JSBSim model does: a real FCC integrates a stored table, not the missile's actual
 * aerodynamics, and the difference between the two is a real property of every DLZ ever flown. The
 * intercept mission measures it — predicted time of flight against the flown one — instead of hiding it
 * by feeding the computation the same numbers the missile flies with.
 *
 * Unguided stores leave this zeroed; nothing reads it for them. */
struct FBWeaponPerf {
  double BoostThrustN = 0.0, BoostS = 0.0;       /* the motor as the FCC knows it */
  double SustainThrustN = 0.0, SustainS = 0.0;
  double LaunchMassKg = 0.0, BurnoutMassKg = 0.0;
  double DragCoefA = 0.0;                        /* supersonic axial-force coefficient, ref area below */
  double RefAreaM2 = 0.0;
  double MinSpeedMs = 0.0;                       /* below it the round can no longer run an intercept */
  double ActivationRangeM = 0.0;                 /* range at which the seeker is switched on (the DLZ's
                                                  * "Radar Activation Range" cue, weapons.md §2.5) */
  double SeekerRangeM = 0.0;                     /* what the seeker can actually detect at */
  double ArmingS = 0.0;                          /* separation + ignition + arming; sets Rmin */
};

struct FBStoreSpec {
  FBStoreKind Kind = FBStoreKind::None;
  const char *Key = "";        /* the mission-file / FBModuleRegistry name of this store */
  const char *FdmModel = "";   /* its JSBSim model directory under the aircraft root */
  bool   Vendored = true;      /* true: the pinned JSBSim submodule's model; false: FlightBox's own
                                * (sim/assets/aircraft — where a model has to live when the submodule
                                * carries none and is read-only, CLAUDE.md Prinzip 1) */
  double MassLbs = 0.0;        /* carriage mass */
  double DragAreaFt2 = 0.0;    /* CdA: carriage drag = this * qbar (lbf), see kMk82 below */
  double MaxFlightS = 0.0;     /* lifetime cap after release (see the runner's retire rule) */

  /* ---- guided-weapon properties. Guided == false leaves every one of them irrelevant, and the whole
   * block zero, which is exactly what a bomb is. ---- */
  bool   Guided = false;       /* flown by modules/missile (seeker + guidance law) rather than by
                                * modules/stores (integrate and fall) */
  bool   RequiresLock = false; /* the SMS refuses the launch without a fire-control solution */
  double FuzeRadiusM = 0.0;    /* proximity fuze: passing a unit closer than this is a hit. 0 = no
                                * proximity fuze at all (a bomb hits what it lands on) */
  FBWeaponPerf Perf;
};

/* Mk-82, 500 lb general-purpose bomb (doc/f16/weapons.md §3).
 *   MassLbs      = the model's own <emptywt> (mk82.xml: 500 LBS). One object, one mass — the figure the
 *                  carrier loses at release is the same one the released FDM instance then flies with.
 *   DragAreaFt2  = the model's own zero-lift drag at carriage Mach, expressed as an area so it can be
 *                  multiplied by the CARRIER's dynamic pressure: mk82.xml's CDmin table gives
 *                  Cd = 0.144 at M 0.8 over <wingarea> 2.54 ft^2 -> CdA = 0.366 ft^2. Deliberately the
 *                  store's own coefficient and NOT a "drag index" out of a loading manual: no T1/T2
 *                  source for one exists (doc/f16/weapons.md §4.5 marks the station/loading figures as
 *                  T4, cross-check only), and interference/pylon drag is a real effect nobody here can
 *                  quantify — so the carriage drag is exactly the store's own body drag, no invented
 *                  installation factor on top.
 *   MaxFlightS   = a leak guard, not physics: a released store that has neither hit anything nor
 *                  diverged after this long is retired so a run cannot accumulate zombie actors. Fall
 *                  times for this class are tens of seconds (§4.2), so 300 s never truncates a real
 *                  trajectory. */
inline constexpr FBStoreSpec kMk82{FBStoreKind::Mk82, "mk82", "mk82", true, 500.0, 0.366, 300.0,
                                   /*Guided*/ false, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0,
                                   FBWeaponPerf{}};

/* AIM-120 AMRAAM (doc/f16/weapons.md §2.5, §3, §4.4). The FIRST guided round: its model is FlightBox's
 * own (Vendored = false -> sim/assets/aircraft/aim120, because the pinned submodule has no AMRAAM and
 * may not be added to), and its module is modules/missile.
 *   MassLbs      = 335 lb launch weight [T3] — the same figure aim120.xml's structure + propellant add
 *                  up to, so what the pylon loses is what the released FDM flies with.
 *   DragAreaFt2  = 0.115 ft^2 [DERIVED]: the model's own subsonic CA (0.43 at carriage Mach 0.8) over
 *                  its 0.2672 ft^2 reference area. Same rule as the Mk-82's — the store's own body drag,
 *                  no invented installation factor.
 *   MaxFlightS   = 120 s [SET]: a leak guard well past any credible engagement (a 40 nm shot arrives
 *                  inside 90 s, see the DLZ integration), never a kill switch on a live trajectory.
 *   FuzeRadiusM  = 10 m [SET]. The AMRAAM's active-radar proximity fuze and its WDU-41/B blast-frag
 *                  warhead's lethal radius are not published with any precision (weapons.md §4.7 lists
 *                  exactly this class of number as a genuine gap). 10 m is the conservative reading of
 *                  a 50 lb blast-fragmentation warhead against a fighter: close enough that it is a HIT
 *                  and not a claim, and small enough that a guidance law which only "roughly" arrives
 *                  does not score one. What a hit DOES is deliberately not modelled yet.
 *   Perf         = the FCC's table (see FBWeaponPerf): thrust/mass out of engine/WPU-6.xml, drag out of
 *                  aim120.xml's CA at Mach 3, and:
 *                  ActivationRangeM 18.5 km (10 nm) [SET] — the DLZ's own "Radar Activation Range" cue
 *                  is per-engagement and has no published constant (§4.4 says so explicitly); 10 nm is
 *                  the doctrinal order and is where this simulator switches the seeker on.
 *                  SeekerRangeM 14.8 km (8 nm) [SET] — likewise unpublished; deliberately SHORTER than
 *                  the activation range, so the seeker is already looking when the target comes into
 *                  its detection range and the handover is a DETECTION event, never a timer.
 *                  MinSpeedMs 340 [SET] — roughly Mach 1 at altitude: below it the round has neither
 *                  the closure nor the q to run an intercept, which is what ends the DLZ integration.
 *                  ArmingS 1.5 [SET] — separation (0.5 s to motor ignition, see FBFdm's throttle slew)
 *                  plus the fuze arming delay; it is what sets Rmin. */
inline constexpr FBStoreSpec kAim120{
    FBStoreKind::Aim120, "aim120", "aim120", false, 335.0, 0.115, 120.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 10.0,
    FBWeaponPerf{/*BoostThrustN*/ 24020.0, /*BoostS*/ 3.0,
                 /*SustainThrustN*/ 6228.0, /*SustainS*/ 7.7,
                 /*LaunchMassKg*/ 152.0, /*BurnoutMassKg*/ 99.8,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.02482,
                 /*MinSpeedMs*/ 340.0,
                 /*ActivationRangeM*/ 18520.0, /*SeekerRangeM*/ 14816.0,
                 /*ArmingS*/ 1.5}};

inline constexpr const FBStoreSpec *kStoreCatalogue[] = {&kMk82, &kAim120};

inline const FBStoreSpec *FBFindStore(const char *key) {
  if (!key) return nullptr;
  for (const FBStoreSpec *s : kStoreCatalogue)
    if (std::strcmp(s->Key, key) == 0) return s;
  return nullptr;
}

inline const FBStoreSpec *FBStoreSpecOf(FBStoreKind kind) {
  for (const FBStoreSpec *s : kStoreCatalogue)
    if (s->Kind == kind) return s;
  return nullptr;
}

/* ONE released store, as the SMS hands it over: which station let go of what, when, and WHERE that
 * station sits relative to the carrier's CG (body axes, metres: +fwd/+right/+down). The offset travels
 * with the release because the SMS is the only thing that knows its own pylon geometry, and the app-side
 * spawn — the only code allowed to produce an FDM (fdm/FBFdmBoot.h) — must place the new unit at the
 * pylon, not at the carrier's centre of gravity. */
struct FBStoreRelease {
  int    Station = 0;
  FBStoreKind Kind = FBStoreKind::None;
  double MassLbs = 0.0;
  double SimTimeS = 0.0;
  double OffFwdM = 0.0, OffRightM = 0.0, OffDownM = 0.0;
  /* THE LAUNCH PROGRAMMING of a guided round: who is shooting and what the shooter's fire control had
   * made of the target at the moment of launch. A missile leaves the rail already knowing where to
   * start looking — that is what makes an inertial midcourse phase possible at all — and it knows whose
   * uplink to listen to for corrections afterwards. Both are zero/invalid for an unguided store. */
  int    LauncherId = 0;
  FBWeaponTargetState Target;
};

} // namespace FlightBox
#endif
