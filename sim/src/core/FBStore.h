/* The STORE catalogue: what an external store IS, as data — mass, drag, a model name.
 * Every number is the store's own JSBSim model or derived from it by a stated formula;
 * derivations: doc/core.md, Abschnitt 7.1. */
#ifndef FBSTORE_H
#define FBSTORE_H

#include <cstdint>
#include <cstring>
#include "FBWeaponUplink.h"

namespace FlightBox {

/* Append only — the ordinal is telemetry-visible; None must stay 0 (a zeroed block = "nothing loaded"). */
enum class FBStoreKind : uint8_t { None = 0, Mk82, Aim120, Aim9, R73, R27r,
                                   /* the surface-to-air rounds (doc/modules/ground/catalogue.md) */
                                   V750, V601, M3m9, M9m33, Strela2, Igla };

/* WHAT KIND OF SEEKER a guided round carries — and therefore which SENSOR SLOT modules/missile gives
 * it, which is the whole difference between the three guided weapons in the tree. Not a behaviour flag
 * on the module: each kind names a derivation of a sensor that already exists, and its tactical
 * property is that sensor's own limits.
 *   ActiveRadar     an sensors/FBRadarSystem of its own: goes active at its own range, after which the
 *                   shooter is free (the AIM-120's "pitbull").
 *   Infrared        an sensors/FBIrstSystem: ANGLES ONLY, no range and no closure, hence pure
 *                   proportional navigation on the measured line-of-sight rate — and deceivable by a
 *                   flare exactly as a radar is by chaff.
 *   SemiActiveRadar an sensors/FBRadarSystem that never transmits: it sees the reflection of the
 *                   SHOOTER's illumination and nothing else, so a broken lock kills it for good.
 *   CommandGuided   NO detector at all: the round is steered from the ground for its whole flight, so
 *                   FBMissileGuidance's strict priority (own seeker > uplink > last known) degenerates
 *                   to its middle branch. It therefore cannot be chaff-decoyed — the cloud seduces the
 *                   SITE's tracking radar instead, which is where such an engagement really breaks.
 * Append only — telemetry-visible through the round's own trace. doc/weapons.md, Spec. */
enum class FBSeekerKind : uint8_t { None = 0, ActiveRadar, Infrared, SemiActiveRadar, CommandGuided };

/* The FIRE-CONTROL COMPUTER'S performance table for a round — deliberately a coarser copy of what the
 * weapon's JSBSim model does, so the prediction error stays measurable rather than tuned away.
 * An unguided store uses the same table and only its four falling-body entries.
 * doc/core.md, Abschnitt 7.1. */
struct FBWeaponPerf {
  double BoostThrustN = 0.0, BoostS = 0.0;
  double SustainThrustN = 0.0, SustainS = 0.0;
  double LaunchMassKg = 0.0, BurnoutMassKg = 0.0;
  double DragCoefA = 0.0;                        /* supersonic axial-force coefficient on RefAreaM2 */
  double RefAreaM2 = 0.0;
  double MinSpeedMs = 0.0;                       /* below it the round can no longer run an intercept */
  double ActivationRangeM = 0.0;                 /* range at which the seeker is switched on */
  double SeekerRangeM = 0.0;
  double ArmingS = 0.0;                          /* separation + arming; sets Rmin, resp. the bomb's arm margin */
};

/* How long the SHOOTER still owes this round its radar, expressed in the one quantity the fire control
 * already publishes (FBFireControlBlock::TimeToActiveS). Three answers, one per seeker kind, and the
 * shooter's Support state reads nothing else:
 *   Infrared        0  — self-guiding from the rail; the obligation ends at launch
 *   ActiveRadar    >0  — until its own seeker goes active at ActivationRangeM (the "pitbull")
 *   SemiActiveRadar -1 — NEVER. The round has no transmitter, so the shooter owes it illumination to
 *                        impact, and the Support state falls back to the predicted time of flight.
 *   CommandGuided   -1 — the same answer for the stronger reason: it has no seeker at all. */
inline double FBSeekerHandoverS(FBSeekerKind k, double timeToActivationS) {
  switch (k) {
    case FBSeekerKind::Infrared: return 0.0;
    case FBSeekerKind::ActiveRadar: return timeToActivationS;
    case FBSeekerKind::SemiActiveRadar:
    case FBSeekerKind::CommandGuided:
    case FBSeekerKind::None: break;
  }
  return -1.0;
}

struct FBStoreSpec {
  FBStoreKind Kind = FBStoreKind::None;
  const char *Key = "";        /* mission-file / FBModuleRegistry name */
  const char *FdmModel = "";
  double MassLbs = 0.0;        /* carriage mass */
  double DragAreaFt2 = 0.0;    /* CdA: carriage drag = this * qbar (lbf) */
  double MaxFlightS = 0.0;     /* lifetime cap after release */

  bool   Guided = false;       /* flown by modules/missile rather than modules/stores */
  bool   RequiresLock = false;
  double FuzeRadiusM = 0.0;    /* 0 = no proximity fuze at all (a bomb hits what it lands on) */
  double WarheadKg = 0.0;      /* explosive+case mass — the ONE store-side input to FBDamageModel */
  FBWeaponPerf Perf;

  /* WHICH SENSOR SLOT modules/missile gives this round — the whole difference between the guided
   * weapons here. Declared AFTER Perf so every entry written before seekers existed keeps its
   * positional initialiser and its defaults. */
  FBSeekerKind Seeker = FBSeekerKind::None;
  double SeekerFovHalfDeg = 0.0;      /* the instantaneous cone the head sees around where it is aimed */
  double SeekerGimbalHalfDeg = 0.0;   /* the MECHANICAL stop, a different and much larger angle */

  /* THE GATHERING PHASE: guidance inhibited for this long after launch, so the round flies the RAIL
   * direction on thrust alone. Zero for every air-launched store, which separates at 250 m/s with full
   * aerodynamic authority; a surface round leaves the rail at zero airspeed and a fin command at 20 m/s
   * would be an authority nobody claims. Declared last, so every entry written before it keeps its
   * positional initialiser. doc/modules/ground/module.md §Spec 4. */
  double GatherS = 0.0;
};

/* Mk-82, 500 lb GP bomb (doc/modules/f16/weapons.md §3). Perf.ArmingS 2.0 [SET], WarheadKg [T3], and
 * Perf.DragCoefA 0.142 [DERIVED, deliberately coarse — the prediction error is the measured thing];
 * every figure derived in doc/core.md, Abschnitt 7.1. */
inline constexpr FBStoreSpec kMk82{FBStoreKind::Mk82, "mk82", "mk82", 500.0, 0.366, 300.0,
                                   /*Guided*/ false, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0,
                                   /*WarheadKg*/ 87.0,
                                   FBWeaponPerf{/*BoostThrustN*/ 0.0, /*BoostS*/ 0.0,
                                                /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                                                /*LaunchMassKg*/ 226.796, /*BurnoutMassKg*/ 226.796,
                                                /*DragCoefA*/ 0.142, /*RefAreaM2*/ 0.235974,
                                                /*MinSpeedMs*/ 0.0,
                                                /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                                                /*ArmingS*/ 2.0}};

/* AIM-120 AMRAAM (doc/modules/f16/weapons.md §2.5, §3, §4.4); FlightBox-own model, flown by modules/missile.
 * FuzeRadiusM/MaxFlightS/MinSpeedMs/ActivationRangeM/SeekerRangeM/ArmingS are [SET] — the published
 * figures are a genuine gap; MassLbs/WarheadKg [T3], DragAreaFt2 [DERIVED].
 * Derivations: doc/core.md, Abschnitt 7.1. */
inline constexpr FBStoreSpec kAim120{
    FBStoreKind::Aim120, "aim120", "aim120", 335.0, 0.115, 120.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 10.0, /*WarheadKg*/ 20.5,
    FBWeaponPerf{/*BoostThrustN*/ 24020.0, /*BoostS*/ 3.0,
                 /*SustainThrustN*/ 6228.0, /*SustainS*/ 7.7,
                 /*LaunchMassKg*/ 152.0, /*BurnoutMassKg*/ 99.8,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.02482,
                 /*MinSpeedMs*/ 340.0,
                 /*ActivationRangeM*/ 18520.0, /*SeekerRangeM*/ 14816.0,
                 /*ArmingS*/ 1.5},
    /*Seeker*/ FBSeekerKind::ActiveRadar, /*FovHalf*/ 10.0, /*GimbalHalf*/ 45.0};

/* ---- THE THREE ROUNDS OF THE ASYMMETRIC-WEAPON STAGE ------------------------------------------
 * All three follow the AIM-120's rules exactly, so only what DIFFERS is argued here:
 *   - the reference area is the body cross-section, S = pi*d^2/4, from each round's documented diameter;
 *   - DragAreaFt2 = 0.43 * S_ft2 [DERIVED] — the same subsonic axial coefficient the shared
 *     slender-body deck carries at carriage Mach, the same rule the AIM-120's 0.115 came from;
 *   - the motor is sized by the ROCKET EQUATION against the documented terminal Mach at Isp 235 s
 *     (ve = 2305 m/s), exactly as the AIM-120's propellant load was: m_p = m0*(1 - 1/exp(dV/ve)),
 *     thrust = m_p*ve/burnS. Burn times are [GAP] in every source and therefore [SET];
 *   - DragCoefA 0.55 is the AIM-120's supersonic axial level, kept because all three fly the same
 *     slender-body aerodynamic deck (each round's own XML, scaled) and a second number would be a
 *     second invention rather than a second measurement. */

/* AIM-9M SIDEWINDER — the F-16's infrared round (doc/modules/f16/weapons.md §2.5, §3, §4.3).
 * Mass 85.3 kg / 188 lb, length 2.85 m, body diameter 0.127 m (5 in), WDU-17/B warhead 9.4 kg, Mk 36
 * motor ~5.2 s: all [T3] (multiply corroborated public figures; the reference base names none of them,
 * which is a source gap recorded there). Max speed > Mach 2.5 and min range ~3,000 ft are [T-ED].
 *   dV to Mach 2.5 over a ~M 0.9 launch = 700 m/s -> m_p = 85.3*(1 - 1/exp(700/2305)) = 22.3 kg,
 *   burnout 63.0 kg, thrust = 22.3*2305/5.2 = 9,885 N.
 * Gimbal +-30 deg [T4, §4.3]; the instantaneous field is the narrow "SPOT" one and no figure exists for
 * it, hence [SET] 2.5 deg — its measurable consequence is that a poorly aimed launch does not acquire.
 * FuzeRadiusM 6.0 [DERIVED from the AIM-120's [SET] 10 m by equal fragment areal density,
 * r ~ sqrt(m): 10*sqrt(9.4/20.5) = 6.8 m, taken down to 6.0 for an annular-blast head that throws its
 * fragments into a band rather than a sphere]. */
inline constexpr FBStoreSpec kAim9{
    FBStoreKind::Aim9, "aim9", "aim9", 188.0, 0.0586, 60.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 6.0, /*WarheadKg*/ 9.4,
    FBWeaponPerf{/*BoostThrustN*/ 9885.0, /*BoostS*/ 5.2,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 85.3, /*BurnoutMassKg*/ 63.0,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.012668,
                 /*MinSpeedMs*/ 200.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 12000.0,
                 /*ArmingS*/ 1.0},
    /*Seeker*/ FBSeekerKind::Infrared, /*FovHalf*/ 2.5, /*GimbalHalf*/ 30.0};

/* R-73 (AA-11 ARCHER) — the MiG-29's infrared round (doc/modules/mig29/weapons.md §3.3).
 * Mass 105 kg, length 2.90 m, diameter 0.17 m, warhead 7.4 kg expanding rod with a documented BLAST
 * RADIUS of 3.5 m, active radio proximity fuze, single-mode solid motor, range 0.3-20 km: all
 * [DCS-FM p.72]. Seeker "Mayak" OGS MK-80, acquisition to 60 deg and gimbal limits later raised to
 * 75 deg [DCS-FM p.71] -> GimbalHalf 75. Burn time is [GAP] -> [SET] 6.0 s; terminal Mach 2.5 class:
 *   dV = 750 m/s -> m_p = 105*(1 - 1/exp(750/2305)) = 29.1 kg, burnout 75.9 kg,
 *   thrust = 29.1*2305/6.0 = 11,180 N.
 * FuzeRadiusM is the DOCUMENTED 3.5 m and not a derivation — the one proximity radius in the tree that
 * is sourced rather than set. The instantaneous field [SET] 5.0 deg, wider than the Sidewinder's for
 * the reason the source itself gives: this seeker can be handed a target far enough off boresight to
 * find it alone, which the R-60M cannot (§3.3). */
inline constexpr FBStoreSpec kR73{
    FBStoreKind::R73, "r73", "r73", 231.5, 0.1051, 60.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 3.5, /*WarheadKg*/ 7.4,
    FBWeaponPerf{/*BoostThrustN*/ 11180.0, /*BoostS*/ 6.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 105.0, /*BurnoutMassKg*/ 75.9,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.022698,
                 /*MinSpeedMs*/ 200.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 20000.0,
                 /*ArmingS*/ 1.0},
    /*Seeker*/ FBSeekerKind::Infrared, /*FovHalf*/ 5.0, /*GimbalHalf*/ 75.0};

/* R-27R (AA-10A ALAMO) — the MiG-29's SEMI-ACTIVE medium-range round, and the reason this stage exists
 * (doc/modules/mig29/weapons.md §3.1, §3.2).
 * Mass 253 kg, length 4.00 m, diameter 0.23 m, 39 kg expanding-rod warhead, documented range 30-35 km,
 * seeker gimbal limit AT LAUNCH 50 deg (SARH): all [DCS-FM p.67-69] -> GimbalHalf 50.
 * MinSpeedMs 250 is the one motor-side figure that IS documented: the class becomes "almost
 * uncontrollable" below 800-1,000 km/h [DCS-FM p.64-65] = 222-278 m/s, midpoint 250 [SET-from-DOC].
 * Burn split [GAP] -> [SET] 3 s boost + 5 s sustain inside the documented class bound of 2-15 s; the
 * class boosts to Mach 2-3, so dV = 850 m/s -> m_p = 253*(1 - 1/exp(850/2305)) = 78.0 kg, burnout
 * 175.0 kg, total impulse 78.0*2305 = 179.8 kN*s, split 60/40 -> boost 35.9 kN over 3 s, sustain
 * 14.4 kN over 5 s.
 * ActivationRangeM is 0 and that is the MECHANIC, not an omission: this round has no transmitter to
 * switch on, so the shooter owes it illumination all the way in (FBSeekerHandoverS -> -1).
 * FuzeRadiusM 13.8 [DERIVED] from the AIM-120's [SET] 10 m at equal fragment areal density,
 * r ~ sqrt(m): 10*sqrt(39/20.5) = 13.8 m. The R-27's own burst radius is an explicit [GAP] in the
 * reference base (§8.3), and it is named there as the one that matters most for BVR damage. */
inline constexpr FBStoreSpec kR27r{
    FBStoreKind::R27r, "r27r", "r27r", 557.8, 0.1923, 90.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 13.8, /*WarheadKg*/ 39.0,
    FBWeaponPerf{/*BoostThrustN*/ 35900.0, /*BoostS*/ 3.0,
                 /*SustainThrustN*/ 14400.0, /*SustainS*/ 5.0,
                 /*LaunchMassKg*/ 253.0, /*BurnoutMassKg*/ 175.0,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.041548,
                 /*MinSpeedMs*/ 250.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 25000.0,
                 /*ArmingS*/ 1.5},
    /*Seeker*/ FBSeekerKind::SemiActiveRadar, /*FovHalf*/ 10.0, /*GimbalHalf*/ 50.0};

/* ---- THE SIX SURFACE-TO-AIR ROUNDS (doc/modules/ground/catalogue.md) -----------------------------
 * One recipe, six rows, and it is the AIM-9/R-73/R-27R recipe unchanged: reference area = the body
 * cross-section from the published diameter, DragAreaFt2 = 0.43 * S_ft2, DragCoefA 0.55 (all of them
 * fly the same slender-body deck), and the motor sized by the rocket equation at Isp 235 s.
 * THE ONE DIFFERENCE, and it is physical: dV is the whole terminal speed instead of a delta over a
 * launch speed, because these leave a RAIL at zero airspeed. Every per-round derivation stands in the
 * round's own XML banner under sim/assets/aircraft/<key>/.
 *
 * RequiresLock is FALSE for all six, and that is not a relaxation: it selects the SMS's air-to-air
 * DLZ interlock (FBStoresSystem::Release), which reads FBFireControlBlock — a box a battery does not
 * have. The envelope that decides a surface launch is the SITE's, four numbers per catalogue row
 * (core/FBSite.h), tested by modules/ground/FBSiteFireControl before it ever presses the button. */

/* V-750V — the S-75's command-guided round. WarheadKg 195 [T4]; FuzeRadiusM 12 [SET], the largest gate
 * in the tree for by far the largest head. MinSpeedMs 300 / MaxFlightS 120 / ArmingS 3.0 [SET]. */
inline constexpr FBStoreSpec kV750{
    FBStoreKind::V750, "v750", "v750", 4768.6, 0.9088, 120.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 12.0, /*WarheadKg*/ 195.0,
    FBWeaponPerf{/*BoostThrustN*/ 239556.0, /*BoostS*/ 4.5,
                 /*SustainThrustN*/ 32667.0, /*SustainS*/ 22.0,
                 /*LaunchMassKg*/ 2163.0, /*BurnoutMassKg*/ 1383.5,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.196350,
                 /*MinSpeedMs*/ 300.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 3.0},
    /*Seeker*/ FBSeekerKind::CommandGuided, /*FovHalf*/ 0.0, /*GimbalHalf*/ 0.0, /*GatherS*/ 3.0};

/* V-601 (5V27) — the S-125's round. WarheadKg 70 [T4], FuzeRadiusM 10 [SET] (the AIM-120's figure for
 * a comparable-order head). MinSpeedMs 280 / MaxFlightS 90 / ArmingS 2.5 [SET]. */
inline constexpr FBStoreSpec kV601{
    FBStoreKind::V601, "v601", "v601", 2101.0, 0.5112, 90.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 10.0, /*WarheadKg*/ 70.0,
    FBWeaponPerf{/*BoostThrustN*/ 179435.0, /*BoostS*/ 2.5,
                 /*SustainThrustN*/ 16614.0, /*SustainS*/ 18.0,
                 /*LaunchMassKg*/ 953.0, /*BurnoutMassKg*/ 628.6,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.110447,
                 /*MinSpeedMs*/ 280.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 2.5},
    /*Seeker*/ FBSeekerKind::CommandGuided, /*FovHalf*/ 0.0, /*GimbalHalf*/ 0.0, /*GatherS*/ 2.5};

/* 3M9 — the 2K12's round, and the ONE surface round FlightBox already had the weapon side for: the
 * SemiActiveRadar seeker built in stage 2c, unchanged. It lives on the site's CW illumination and dies
 * with it, for good. WarheadKg 59 [T4], FuzeRadiusM 8 [SET]. SeekerRangeM 20 000 [SET, inside the
 * sourced 28 km illumination range]. MinSpeedMs 250 / MaxFlightS 90 / ArmingS 2.0 [SET]. */
inline constexpr FBStoreSpec k3m9{
    FBStoreKind::M3m9, "3m9", "3m9", 1320.6, 0.3959, 90.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 8.0, /*WarheadKg*/ 59.0,
    FBWeaponPerf{/*BoostThrustN*/ 51978.0, /*BoostS*/ 4.0,
                 /*SustainThrustN*/ 11551.0, /*SustainS*/ 18.0,
                 /*LaunchMassKg*/ 599.0, /*BurnoutMassKg*/ 418.6,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.085530,
                 /*MinSpeedMs*/ 250.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 20000.0,
                 /*ArmingS*/ 2.0},
    /*Seeker*/ FBSeekerKind::SemiActiveRadar, /*FovHalf*/ 10.0, /*GimbalHalf*/ 45.0, /*GatherS*/ 2.0};

/* 9M33 — the 9K33's round. WarheadKg 19 [T4] (the earlier variant), FuzeRadiusM 6 [SET] (the AIM-9's
 * figure for a comparable head). MinSpeedMs 200 / MaxFlightS 60 / ArmingS 1.5 [SET]. */
inline constexpr FBStoreSpec k9m33{
    FBStoreKind::M9m33, "9m33", "9m33", 277.8, 0.1597, 60.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 6.0, /*WarheadKg*/ 19.0,
    FBWeaponPerf{/*BoostThrustN*/ 27028.0, /*BoostS*/ 2.0,
                 /*SustainThrustN*/ 2772.0, /*SustainS*/ 13.0,
                 /*LaunchMassKg*/ 126.0, /*BurnoutMassKg*/ 86.9,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.034504,
                 /*MinSpeedMs*/ 200.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 1.5},
    /*Seeker*/ FBSeekerKind::CommandGuided, /*FovHalf*/ 0.0, /*GimbalHalf*/ 0.0, /*GatherS*/ 1.5};

/* 9M32M STRELA-2M — the rear-aspect MANPADS round, FBSeekerKind::Infrared unchanged. WarheadKg 1.15
 * [T4], FuzeRadiusM 2.0 [SET] — below the R-73's documented 3.5 m for a head several times larger.
 * SeekerRangeM 4 200 = its own published envelope [T4], which is what makes the aspect law bite: the
 * head's reach is scaled by the target's infrared aspect, so 4.2 km astern is ~1.7 km head-on and the
 * "revenge weapon" property falls out of the existing law rather than out of a new gate.
 * FovHalf 0.95 [T4, the documented 1.9 deg field]; GimbalHalf 30 [SET]. MinSpeedMs 120 /
 * MaxFlightS 30 / ArmingS 0.6 [SET]. */
inline constexpr FBStoreSpec kStrela2{
    FBStoreKind::Strela2, "strela2", "strela2", 21.6, 0.0188, 30.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 2.0, /*WarheadKg*/ 1.15,
    FBWeaponPerf{/*BoostThrustN*/ 2202.0, /*BoostS*/ 2.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 9.8, /*BurnoutMassKg*/ 7.9,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.004072,
                 /*MinSpeedMs*/ 120.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 4200.0,
                 /*ArmingS*/ 0.6},
    /*Seeker*/ FBSeekerKind::Infrared, /*FovHalf*/ 0.95, /*GimbalHalf*/ 30.0, /*GatherS*/ 0.6};

/* 9M39 IGLA — the all-aspect MANPADS round. It differs from the Strela in exactly two numbers, its
 * reach and its head's field, and NOT in flare resistance: FlightBox's flare model is an irradiance
 * inequality with no rejection term, so the Igla is here as flare-defeatable as the Strela. That is a
 * declared understatement (doc/modules/ground/module.md G8), not an oversight. WarheadKg 1.17 [T4],
 * SeekerRangeM 5 200 [T4], FovHalf 2.0 / GimbalHalf 40 [SET]. */
inline constexpr FBStoreSpec kIgla{
    FBStoreKind::Igla, "igla", "igla", 23.8, 0.0188, 30.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 2.0, /*WarheadKg*/ 1.17,
    FBWeaponPerf{/*BoostThrustN*/ 2727.0, /*BoostS*/ 2.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 10.8, /*BurnoutMassKg*/ 8.4,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.004072,
                 /*MinSpeedMs*/ 130.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 5200.0,
                 /*ArmingS*/ 0.6},
    /*Seeker*/ FBSeekerKind::Infrared, /*FovHalf*/ 2.0, /*GimbalHalf*/ 40.0, /*GatherS*/ 0.6};

inline constexpr const FBStoreSpec *kStoreCatalogue[] = {&kMk82, &kAim120, &kAim9, &kR73, &kR27r,
                                                         &kV750, &kV601, &k3m9, &k9m33, &kStrela2,
                                                         &kIgla};

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

/* Append only — the ordinal is the mission-visible `set attack_mode` value and a telemetry column. */
enum class FBDeliveryMode : uint8_t { Ccip = 0, Ccrp };

inline const char *FBDeliveryModeStr(FBDeliveryMode m) {
  return m == FBDeliveryMode::Ccrp ? "ccrp" : "ccip";
}

/* What an unguided release was aimed with — a RECORD, nothing here steers: the prediction leaves the
 * jet with the weapon so the measured impact can be put beside it (doc/core.md, 7.2). */
struct FBReleaseSolution {
  bool   Valid = false;
  FBDeliveryMode Mode = FBDeliveryMode::Ccip;
  double ImpactLatDeg = 0.0, ImpactLonDeg = 0.0;   /* where the computer said it would land */
  double ImpactElevM = 0.0;                        /* the plane it solved against */
  double TofS = 0.0;
  double AimLatDeg = 0.0, AimLonDeg = 0.0;         /* the designated point */
  double AimMissM = 0.0;                           /* predicted impact -> aim point, at release */
  double ArmMarginS = 0.0;                         /* < 0 = released below the arming margin (a dud) */
  /* Necessarily the PREVIOUS sweep's solution (bus ordering) — recorded, not hidden, so a measurement
   * can state how much of its error is the age of the number. */
  double StampS = 0.0;
};

/* ONE released store, as the SMS hands it over. The station offset (body axes, m, +fwd/+right/+down)
 * travels along because only the SMS knows its pylon geometry and the app-side spawn must place the new
 * unit at the pylon, not at the carrier's CG. */
struct FBStoreRelease {
  int    Station = 0;
  FBStoreKind Kind = FBStoreKind::None;
  double MassLbs = 0.0;
  double SimTimeS = 0.0;
  double OffFwdM = 0.0, OffRightM = 0.0, OffDownM = 0.0;
  /* Launch programming of a guided round — where to start looking and whose uplink to listen to.
   * Zero/invalid for an unguided store, which carries the delivery Solution instead. */
  int    LauncherId = 0;
  FBWeaponTargetState Target;
  FBReleaseSolution Solution;
  /* THE RAIL, for a launcher that HAS one. A store leaves a pylon with the carrier's attitude, which is
   * why this is false for every air-launched release and the spawn path is untouched by it; a surface
   * round leaves a rail that was ELEVATED and TRAINED, and the position's own body attitude (roll =
   * pitch = 0, yaw = its mount heading) says nothing about where the launcher points. */
  bool   HaveRail = false;
  double RailPitchDeg = 0.0, RailYawDeg = 0.0;
};

} // namespace FlightBox
#endif
