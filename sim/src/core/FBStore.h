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

/* THE FIRE-CONTROL COMPUTER'S OWN PERFORMANCE TABLE for a round — the coarse model a launch-zone or a
 * bomb-fall computation runs on (modules/f16/FBF16FireControl). It is DELIBERATELY a separate,
 * simplified copy of what the weapon's JSBSim model does: a real FCC integrates a stored table, not the
 * weapon's actual aerodynamics, and the difference between the two is a real property of every DLZ ever
 * flown and every CCIP pipper ever put on a target. The intercept mission measures it for the guided
 * case — predicted time of flight against the flown one — and the ground-attack missions for the
 * unguided one, instead of hiding it by feeding the computation the same numbers the weapon flies with.
 *
 * AN UNGUIDED STORE USES THE SAME TABLE, and only the four entries a falling body has: LaunchMassKg,
 * DragCoefA, RefAreaM2 and ArmingS. Its motor fields stay zero because it has no motor, and its seeker
 * fields because it has no seeker — the table is not "the guided block", it is what the computer knows
 * about the round (core/FBBallistics.h integrates exactly these four). */
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
  /* Separation + arming. For a guided round it also covers motor ignition and is what sets Rmin; for a
   * bomb it is the fall time the fuze needs before it will function, i.e. the pull-up anticipation cue's
   * own number (core/FBBallistics.h's ArmMarginS). One quantity, one field. */
  double ArmingS = 0.0;
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
  double WarheadKg = 0.0;      /* explosive+case mass — the ONE store-side input to the damage model
                                * (core/FBDamageModel). 0 = an inert round that hurts nothing */
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
/*   WarheadKg    = 87 kg (192 lb) of Tritonal [T3, the Mk-82's standard fill]. A bomb has no proximity
 *                  fuze, so nothing resolves a Mk-82 burst against an aircraft; what DOES read it is the
 *                  ground burst at the impact point (app/FBMissionRunner.cpp), through the same
 *                  core/FBDamageModel a warhead detonating beside an aircraft goes through.
 *   Perf         = the FCC's BALLISTIC table (see FBWeaponPerf): what the fire-control computer knows
 *                  about this bomb, and deliberately less than the bomb knows about itself.
 *                  LaunchMassKg 226.8 = the model's own 500 lb, one object one mass.
 *                  RefAreaM2 0.23597 = the model's own <wingarea> (2.54 ft^2), the area its whole drag
 *                  table is referred to.
 *                  DragCoefA 0.142 [DERIVED, and deliberately coarse]: mk82.xml's CDmin table runs
 *                  0.140 at M 0.2 through 0.144 at M 0.8 and then rises steeply transonically; the
 *                  computer carries ONE subsonic number, which is what a stored table is, and the
 *                  resulting prediction error against the model's own Mach-dependent drag is the thing
 *                  the CCIP/CCRP missions measure rather than tune away.
 *                  ArmingS 2.0 [SET]: no citable arming delay exists for the Mk-82's standard fuzes
 *                  (weapons.md §4.7 marks fuze internals as a gap, and §4.2's PUAC text gives the
 *                  CONCEPT without a number); 2 s is the order of a nose fuze's arming vane and it is
 *                  what the pull-up anticipation cue is computed from. */
inline constexpr FBStoreSpec kMk82{FBStoreKind::Mk82, "mk82", "mk82", true, 500.0, 0.366, 300.0,
                                   /*Guided*/ false, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0,
                                   /*WarheadKg*/ 87.0,
                                   FBWeaponPerf{/*BoostThrustN*/ 0.0, /*BoostS*/ 0.0,
                                                /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                                                /*LaunchMassKg*/ 226.796, /*BurnoutMassKg*/ 226.796,
                                                /*DragCoefA*/ 0.142, /*RefAreaM2*/ 0.235974,
                                                /*MinSpeedMs*/ 0.0,
                                                /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                                                /*ArmingS*/ 2.0}};

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
 *                  does not score one. WHAT a hit does is core/FBDamageModel's, out of the next figure.
 *   WarheadKg    = 20.5 kg (45 lb), the WDU-41/B blast-fragmentation warhead [T3 — the published figure
 *                  is consistently "about 40-50 lb" and §4.7 marks warhead internals as a genuine gap].
 *                  It is the ONE weapon-side number the damage model reads; everything else about a hit
 *                  comes from the measured geometry of the burst.
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
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 10.0, /*WarheadKg*/ 20.5,
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

/* THE DELIVERY MODE an unguided release was computed in (doc/f16/weapons.md §2.5). Append only — the
 * ordinal is the mission-visible `set attack_mode` value and a telemetry column. */
enum class FBDeliveryMode : uint8_t { Ccip = 0, Ccrp };

inline const char *FBDeliveryModeStr(FBDeliveryMode m) {
  return m == FBDeliveryMode::Ccrp ? "ccrp" : "ccip";
}

/* WHAT AN UNGUIDED RELEASE WAS AIMED WITH — the fire control's own answer at the instant the pickle was
 * accepted, carried out of the aircraft on the round. The exact counterpart of FBWeaponTargetState for
 * a guided launch, and it exists for the same reason: the prediction has to leave the jet WITH the
 * weapon, so the owner of the simulation can put it beside the impact it then measures and state the
 * error. Nothing in here steers anything — a bomb has no guidance; it is a record. */
struct FBReleaseSolution {
  bool   Valid = false;
  FBDeliveryMode Mode = FBDeliveryMode::Ccip;
  double ImpactLatDeg = 0.0, ImpactLonDeg = 0.0;   /* where the computer said it would land */
  double ImpactElevM = 0.0;                        /* the plane it solved against */
  double TofS = 0.0;
  double AimLatDeg = 0.0, AimLonDeg = 0.0;         /* what it was aimed AT (the designated point) */
  double AimMissM = 0.0;                           /* predicted impact -> aim point, at release */
  double ArmMarginS = 0.0;                         /* < 0 = released below the arming margin (a dud) */
  /* WHEN the computer produced it. A release is answered by the SMS in the module's Stores command group,
   * which is serviced BEFORE the fire control's own tick in the same sensor sweep (modules/f16/
   * FBF16Module's rate table), so the solution a round is stamped with is necessarily the previous
   * sweep's. That lag is a real property of the bus ordering and it is worth tens of metres at fighter
   * speeds — so it is RECORDED rather than hidden, and every measurement made from this struct can state
   * how much of its error is simply the age of the number. */
  double StampS = 0.0;
};

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
  /* ...and the unguided half of the same idea: the delivery solution the release was made on (see
   * FBReleaseSolution). Invalid for a guided round, which is aimed by its seeker and not by a table. */
  FBReleaseSolution Solution;
};

} // namespace FlightBox
#endif
