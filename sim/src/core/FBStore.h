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
                                   V750, V601, M3m9, M9m33, Strela2, Igla,
                                   /* the air-to-ground stores (doc/air-to-ground.md §§2-3) */
                                   Agm88, Mk84, Gbu12, Cbu87, Fab250, Fab500,
                                   /* the catalogue aircraft's rounds (doc/modules/air/catalogue.md) */
                                   K13, R60, R24r, R40r, Aim7, S530f, Magic1 };

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
 *   SemiActiveLaser an ILLUMINATOR of the shooter's again, but a passive energy detector rather than a
 *                   receiver tuned to one transmitter: it reacquires a spot that comes back, which is
 *                   the one place it differs from SemiActiveRadar. Its law is not proportional
 *                   navigation but the documented Paveway PURSUIT law (doc/air-to-ground.md §3.2).
 *   AntiRadiation   a sensors/FBRwrSystem of its own: a bearing, an elevation and a received power, and
 *                   NEVER a range. It has no transmitter, no interrogator and no identity, so it homes
 *                   on whatever of its programmed class is loudest inside its cone — friend included —
 *                   and it loses its target the moment that target stops transmitting.
 * Append only — telemetry-visible through the round's own trace. doc/weapons.md, Spec. */
enum class FBSeekerKind : uint8_t { None = 0, ActiveRadar, Infrared, SemiActiveRadar, CommandGuided,
                                    SemiActiveLaser, AntiRadiation };

/* WHAT AN ANTI-RADIATION ROUND WAS PROGRAMMED TO HOME ON — the honest coarsening of a threat table
 * FlightBox has no data for: FBEmitterSignature carries no band letter, no PRF and no pulse width, so
 * the filter is three values over FBEmitterKind. `[SET]` per mission (`set arm_class`).
 * doc/air-to-ground.md §2.1. */
enum class FBArTargetClass : uint8_t { AnySurface = 0, SurfaceFireControl, SurfaceEarlyWarning };

inline const char *FBArTargetClassStr(FBArTargetClass c) {
  switch (c) {
    case FBArTargetClass::AnySurface: return "any";
    case FBArTargetClass::SurfaceFireControl: return "fire_control";
    case FBArTargetClass::SurfaceEarlyWarning: return "early_warning";
  }
  return "?";
}

inline bool FBArTargetClassFromString(const char *s, FBArTargetClass &out) {
  if (!s) return false;
  if (std::strcmp(s, "any") == 0) { out = FBArTargetClass::AnySurface; return true; }
  if (std::strcmp(s, "fire_control") == 0) { out = FBArTargetClass::SurfaceFireControl; return true; }
  if (std::strcmp(s, "early_warning") == 0) { out = FBArTargetClass::SurfaceEarlyWarning; return true; }
  return false;
}

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
 *   CommandGuided   -1 — the same answer for the stronger reason: it has no seeker at all.
 *   SemiActiveLaser -1 — the illuminator is the shooter's, so it owes the round a spot to impact.
 *   AntiRadiation    0 — the target IS the transmitter; the shooter owes it nothing at all. */
inline double FBSeekerHandoverS(FBSeekerKind k, double timeToActivationS) {
  switch (k) {
    case FBSeekerKind::Infrared:
    case FBSeekerKind::AntiRadiation: return 0.0;
    case FBSeekerKind::ActiveRadar: return timeToActivationS;
    case FBSeekerKind::SemiActiveRadar:
    case FBSeekerKind::SemiActiveLaser:
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

  /* ---- THE AIR-TO-GROUND FIELDS. All appended, all defaulting to "this row has none", which is what
   * keeps every entry above byte-identical in behaviour. doc/air-to-ground.md §§2-3. ---- */

  /* How long an ANGLE-ONLY round holds the last measured line-of-sight RATE after its target stops
   * being measurable. A RATE and never a POINT: when it expires the rate is zero, proportional
   * navigation commands nothing lateral and the gravity bias alone survives, so the round coasts
   * STRAIGHT. That is literally "stop steering now", which makes the resulting miss the zero-effort
   * miss of the law already in the tree. 0 = the guidance's generic kLosRateHoldS. */
  double SeekerMemoryS = 0.0;

  /* THE CARRIAGE ENVELOPE the STORE imposes on its own release, in calibrated airspeed. A property of
   * the store and not of the airframe or of its computer, so it is checked after both. 0 = no limit,
   * i.e. every row written before this field existed is unconstrained by construction. */
  double ReleaseMinKt = 0.0, ReleaseMaxKt = 0.0;

  /* A CLUSTER: N submunitions of m each over a fixed rectangle, long axis along the canister's arrival
   * track. Zero = a point source, resolved by the fragment law like every other warhead. The pattern is
   * NOT a function of release altitude or speed (doc/air-to-ground.md N2), so where the canister
   * functions makes no difference to what arrives — which is why it is laid at the ground crossing and
   * needs no burst-altitude field. */
  int    SubCount = 0;
  double SubMassKg = 0.0;
  double FootAlongM = 0.0, FootAcrossM = 0.0;
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

/* ---- THE AIR-TO-GROUND STORES (doc/air-to-ground.md §§2-3) ---------------------------------------
 * The four free-fall rows are the Mk-82's recipe scaled to their own diameter and mass, one relation
 * each and no second aerodynamic invention: the deck is NON-DIMENSIONAL (every coefficient multiplies
 * qbar * Sw), so a bigger body is a bigger Sw and a heavier mass and nothing else.
 *   RefAreaM2 = 0.235974 * (d/d_mk82)^2,  DragAreaFt2 = 0.366 * (d/d_mk82)^2,  d_mk82 = 10.75 in
 *   DragCoefA 0.142 unchanged — the Mk-82's own coarse figure, kept because all four fly the same deck
 * and a second number would be a second invention rather than a second measurement.
 * ALL FOUR INHERIT THE MK-82 DECK'S DECLARED FIDELITY CAVEAT IN FULL (doc/modules/stores.md Gaps 1):
 * the delivery error measured against them is fidelity to the MODEL, never to a real release. */

/* AGM-88 HARM — the anti-radiation round, and the whole reason doc/air-to-ground.md exists.
 * Mass 355 kg / 780 lb, diameter 254 mm, WDU-21/B warhead 66.0 kg: all [T4]. Terminal Mach 1.84
 * [T2, ED p.34-42] is carried instead of the [T4] Mach 2.9 because the rest of the F-16 reference base
 * is built from the ED figures and mixing sources inside one deck is worse than being slow.
 *   dV to Mach 1.84 over a M 0.9 launch at 11 km (a = 295.1 m/s) = 543.0 - 265.6 = 277.4 m/s
 *   m_p = 353.8*(1 - 1/exp(277.4/2305)) = 40.1 kg, burnout 313.7 kg, burn [SET] 6.0 s
 *   thrust = 40.1*2305/6.0 = 15 405 N
 * FuzeRadiusM 5.0 [SET]: the weapon's fuze is a LASER proximity one [T2] and no radius is published;
 * 5 m is the smallest gate in the tree above the MANPADS rows, chosen because a HARM's lethality is its
 * fragment pattern on a soft van and not a direct hit, and because the fuze radius is a did-it-detonate
 * gate while the MASS carries the lethality (doc/weapons.md §6.1).
 * SeekerFovHalfDeg 40 [SET]: the HAS field-of-view options are documented and their angles are not
 * [T2]; it must exceed the off-boresight angle a launch can produce, and its measurable consequence is
 * that a shot taken further off the nose than this never acquires. GimbalHalf is the SAME angle and
 * that is physics: a wideband passive receiver has no antenna to point.
 * SeekerMemoryS 4.0 [SET] — the one number in this weapon that decides a shot, and it is logged.
 * NO Rmax and no ActivationRangeM: RequiresLock is false, so this round has no DLZ at all and its reach
 * is whatever its own deck produces (doc/weapons.md §4.2). SeekerRangeM 0 for the same reason — a
 * passive receiver's reach is the EMITTER's gate times the one-way advantage, held by FBRwrSystem. */
inline constexpr FBStoreSpec kAgm88{
    FBStoreKind::Agm88, "agm88", "agm88", 780.0, 0.2345, 120.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 5.0, /*WarheadKg*/ 66.0,
    FBWeaponPerf{/*BoostThrustN*/ 15405.0, /*BoostS*/ 6.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 353.8, /*BurnoutMassKg*/ 313.7,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.050671,
                 /*MinSpeedMs*/ 200.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 1.5},
    /*Seeker*/ FBSeekerKind::AntiRadiation, /*FovHalf*/ 40.0, /*GimbalHalf*/ 40.0, /*GatherS*/ 0.0,
    /*SeekerMemoryS*/ 4.0};

/* Mk-84, 2 000 lb GP bomb. MassLbs 2 039 nominal and WarheadKg 428.6 (945 lb Tritonal) [T4],
 * corroborated by [T2] doc/modules/f16/weapons.md §3. Diameter 18 in, length 151.2 in.
 * It is worth a factor of 2.2 in lethal radius over the Mk-82 against BOTH fragility classes
 * (doc/air-to-ground.md §Knowledge 2), which is the entire reason to carry one instead of two. */
inline constexpr FBStoreSpec kMk84{
    FBStoreKind::Mk84, "mk84", "mk84", 2039.0, 1.0261, 300.0,
    /*Guided*/ false, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0, /*WarheadKg*/ 428.6,
    FBWeaponPerf{/*BoostThrustN*/ 0.0, /*BoostS*/ 0.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 924.9, /*BurnoutMassKg*/ 924.9,
                 /*DragCoefA*/ 0.142, /*RefAreaM2*/ 0.661593,
                 /*MinSpeedMs*/ 0.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 2.0}};

/* GBU-12 PAVEWAY II — a Mk-82 with a guidance kit, and therefore a SEMI-ACTIVE weapon whose
 * illuminator is the shooter. WarheadKg is the Mk-82's 87.0 unchanged: an LGB changes NOTHING about
 * lethality and everything about whether the delivery error is inside it. Mass 610 lb [T4].
 * DragAreaFt2 0.45 [SET] = the Mk-82's 0.366 plus the kit's canards and strakes.
 * SeekerFovHalfDeg 15 / GimbalHalf 30 [SET]: no figure is published for either; the consequence is the
 * point — a release that puts the spot outside the head's field does not acquire. SeekerRangeM 10 000
 * [SET]: the range at which a reflected spot is detectable, well beyond any release this tree flies.
 * FuzeRadiusM 0 — it is a bomb: it hits what it lands on. */
inline constexpr FBStoreSpec kGbu12{
    FBStoreKind::Gbu12, "gbu12", "gbu12", 610.0, 0.2710, 300.0,
    /*Guided*/ true, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0, /*WarheadKg*/ 87.0,
    FBWeaponPerf{/*BoostThrustN*/ 0.0, /*BoostS*/ 0.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 276.7, /*BurnoutMassKg*/ 276.7,
                 /*DragCoefA*/ 0.55, /*RefAreaM2*/ 0.058552,
                 /*MinSpeedMs*/ 0.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 10000.0,
                 /*ArmingS*/ 2.0},
    /*Seeker*/ FBSeekerKind::SemiActiveLaser, /*FovHalf*/ 15.0, /*GimbalHalf*/ 30.0};

/* CBU-87/B COMBINED EFFECTS MUNITION — the one store in the tree that is an AREA rather than a point.
 * SUU-65/B canister 950 lb (430 kg), 202 x BLU-97/B of 1.5 kg [T3]/[T4]; diameter 15.6 in.
 * Footprint 200 x 400 m [T2 Chuck / T4] against 244 x 122 m [T3 GlobalSecurity] — [DISPUTED], both
 * carried, and FlightBox takes the LARGER because it is the more conservative reading of the lethality
 * (a thinner areal density over a bigger rectangle).
 * WarheadKg is deliberately 0: this store has no point warhead at all, and expressing the submunitions
 * as one would be the invention. Its whole effect is the areal energy density of §Knowledge 3, and its
 * weakness is stated there and booked as N3: the verdict against `target_soft` sits 12 % above the
 * failure threshold, so two [SET] constants of the damage model decide it. */
inline constexpr FBStoreSpec kCbu87{
    FBStoreKind::Cbu87, "cbu87", "cbu87", 950.0, 0.7708, 300.0,
    /*Guided*/ false, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0, /*WarheadKg*/ 0.0,
    FBWeaponPerf{/*BoostThrustN*/ 0.0, /*BoostS*/ 0.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 430.9, /*BurnoutMassKg*/ 430.9,
                 /*DragCoefA*/ 0.142, /*RefAreaM2*/ 0.496881,
                 /*MinSpeedMs*/ 0.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 2.0},
    /*Seeker*/ FBSeekerKind::None, /*FovHalf*/ 0.0, /*GimbalHalf*/ 0.0, /*GatherS*/ 0.0,
    /*SeekerMemoryS*/ 0.0, /*ReleaseMinKt*/ 0.0, /*ReleaseMaxKt*/ 0.0,
    /*SubCount*/ 202, /*SubMassKg*/ 1.5, /*FootAlongM*/ 400.0, /*FootAcrossM*/ 200.0};

/* FAB-250 M-62 / FAB-500 M-62 — the Soviet free-fall pair, and the FIRST stores in the tree whose
 * CARRIAGE refuses a release. Class mass and filling [T3] via doc/modules/mig29/weapons.md §5.1;
 * FAB-500 2 470 x 400 mm, FAB-250 ~1 500 x 285 mm.
 * THE RELEASE ENVELOPE 500-1 000 km/h is [T2, DCS-FM p.75] and the MiG-29 reference base already rules
 * it "a rejectable condition (out_of_context), not a guideline" — 269.98 / 539.96 kt [DERIVED].
 * CARRIAGE AND BRIEFED RELEASE ONLY: the MiG-29 cannot fly `set task attack` at all (C9 — the real
 * jet's unguided delivery is a DIRECTOR, not a release cue), so these rows exist and their delivery
 * mode does not. */
inline constexpr FBStoreSpec kFab250{
    FBStoreKind::Fab250, "fab250", "fab250", 551.2, 0.3987, 300.0,
    /*Guided*/ false, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0, /*WarheadKg*/ 100.0,
    FBWeaponPerf{/*BoostThrustN*/ 0.0, /*BoostS*/ 0.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 250.0, /*BurnoutMassKg*/ 250.0,
                 /*DragCoefA*/ 0.142, /*RefAreaM2*/ 0.257067,
                 /*MinSpeedMs*/ 0.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 2.0},
    /*Seeker*/ FBSeekerKind::None, /*FovHalf*/ 0.0, /*GimbalHalf*/ 0.0, /*GatherS*/ 0.0,
    /*SeekerMemoryS*/ 0.0, /*ReleaseMinKt*/ 269.98, /*ReleaseMaxKt*/ 539.96};

inline constexpr FBStoreSpec kFab500{
    FBStoreKind::Fab500, "fab500", "fab500", 1102.3, 0.7854, 300.0,
    /*Guided*/ false, /*RequiresLock*/ false, /*FuzeRadiusM*/ 0.0, /*WarheadKg*/ 201.0,
    FBWeaponPerf{/*BoostThrustN*/ 0.0, /*BoostS*/ 0.0,
                 /*SustainThrustN*/ 0.0, /*SustainS*/ 0.0,
                 /*LaunchMassKg*/ 500.0, /*BurnoutMassKg*/ 500.0,
                 /*DragCoefA*/ 0.142, /*RefAreaM2*/ 0.506354,
                 /*MinSpeedMs*/ 0.0,
                 /*ActivationRangeM*/ 0.0, /*SeekerRangeM*/ 0.0,
                 /*ArmingS*/ 2.0},
    /*Seeker*/ FBSeekerKind::None, /*FovHalf*/ 0.0, /*GimbalHalf*/ 0.0, /*GatherS*/ 0.0,
    /*SeekerMemoryS*/ 0.0, /*ReleaseMinKt*/ 269.98, /*ReleaseMaxKt*/ 539.96};

/* ---- THE SEVEN AIR-TO-AIR ROUNDS OF THE CATALOGUE AIRCRAFT (doc/modules/air/catalogue.md) ---------
 * One recipe, seven rows, and it is the AIM-9/R-73/R-27R recipe unchanged — reference area = the body
 * cross-section from the published diameter, DragAreaFt2 = 0.43 * S_ft2, DragCoefA 0.55 (they all fly
 * the same slender-body deck), the motor sized by the rocket equation at Isp 235 s over a DELTA above
 * the 250 m/s separation speed. Every number below is printed by `tools/gen_air_stores.py --cpp`, which
 * also writes the decks, so the catalogue row and the XML cannot say two different things.
 * FuzeRadiusM is [DERIVED] from the AIM-120's [SET] 10 m at 20.5 kg by r ~ sqrt(m), the identical
 * relation the R-27R row above uses.
 *
 * FOUR OF THE SEVEN ARE SEMI-ACTIVE and therefore BIND THE SHOOTER TO IMPACT (FBSeekerHandoverS -1).
 * That is the catalogue's normal case rather than its exception, and it is why doc/weapons.md needed
 * NO new seeker kind for any of this. */

/* K-13 / R-13M. The read source gives 0.97-3.5 km, which is SHORT against commonly quoted R-13M
 * figures; carried as read and marked [TODO] for a second source (catalogue D9). Rear-aspect only —
 * expressed by the 40 deg gimbal plus the infrared aspect law the seeker slot already runs. */
inline constexpr FBStoreSpec kK13{
    FBStoreKind::K13, "k13", "k13", 198.4, 0.0586, 60.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 6.0, /*WarheadKg*/ 7.4,
    FBWeaponPerf{7912.0, 5.0, 0.0, 0.0, 90.0, 72.8, 0.55, 0.012668, 200.0, 0.0, 3500.0, 1.0},
    /*Seeker*/ FBSeekerKind::Infrared, /*FovHalf*/ 5.0, /*GimbalHalf*/ 40.0};

/* R-60: the short, light, wide-gimbal round four eastern rows carry. */
inline constexpr FBStoreSpec kR60{
    FBStoreKind::R60, "r60", "r60", 97.0, 0.0523, 60.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 3.8, /*WarheadKg*/ 3.0,
    FBWeaponPerf{6341.0, 3.0, 0.0, 0.0, 44.0, 35.7, 0.55, 0.011310, 200.0, 0.0, 8000.0, 0.8},
    /*Seeker*/ FBSeekerKind::Infrared, /*FovHalf*/ 5.0, /*GimbalHalf*/ 45.0};

/* R-24R, "comparable to the AIM-7 Sparrow" [T4] — the MiG-23's BVR round, SARH, bound to impact. */
inline constexpr FBStoreSpec kR24r{
    FBStoreKind::R24r, "r24r", "r24r", 489.4, 0.1808, 90.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 11.0, /*WarheadKg*/ 25.0,
    FBWeaponPerf{24650.0, 3.0, 9860.0, 5.0, 222.0, 168.5, 0.55, 0.039057, 250.0, 0.0, 35000.0, 1.5},
    /*Seeker*/ FBSeekerKind::SemiActiveRadar, /*FovHalf*/ 10.0, /*GimbalHalf*/ 50.0};

/* R-40R: the biggest air-to-air round in the tree by a factor of two, and the reason the MiG-25 is
 * worth having even though it cannot turn. WarheadKg 38 is the LOWER bound of the sourced 38-100 kg. */
inline constexpr FBStoreSpec kR40r{
    FBStoreKind::R40r, "r40r", "r40r", 1047.2, 0.3493, 120.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 13.6, /*WarheadKg*/ 38.0,
    FBWeaponPerf{56226.0, 4.0, 23001.0, 8.0, 475.0, 297.6, 0.55, 0.075477, 300.0, 0.0, 80000.0, 2.0},
    /*Seeker*/ FBSeekerKind::SemiActiveRadar, /*FovHalf*/ 10.0, /*GimbalHalf*/ 50.0};

/* AIM-7F/M Sparrow. THE PAIRING THIS ROUND EXISTS FOR: an F-15C with Sparrows fights the MiG-29's
 * bound fight (cannot defend while supporting) and the same aircraft with AMRAAMs fights the F-16's. */
inline constexpr FBStoreSpec kAim7{
    FBStoreKind::Aim7, "aim7", "aim7", 509.3, 0.1498, 90.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 14.0, /*WarheadKg*/ 40.0,
    FBWeaponPerf{35363.0, 3.0, 11788.0, 6.0, 231.0, 154.3, 0.55, 0.032365, 250.0, 0.0, 70000.0, 1.5},
    /*Seeker*/ FBSeekerKind::SemiActiveRadar, /*FovHalf*/ 10.0, /*GimbalHalf*/ 50.0};

/* Super 530F: the Mirage F1's SARH round, and the western half of W2's problem. */
inline constexpr FBStoreSpec kS530f{
    FBStoreKind::S530f, "s530f", "s530f", 540.1, 0.2514, 60.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 12.1, /*WarheadKg*/ 30.0,
    FBWeaponPerf{54837.0, 2.5, 18455.0, 4.0, 245.0, 153.5, 0.55, 0.054325, 250.0, 0.0, 25000.0, 1.5},
    /*Seeker*/ FBSeekerKind::SemiActiveRadar, /*FovHalf*/ 10.0, /*GimbalHalf*/ 50.0};

/* R550 Magic 1: the widest gimbal in the catalogue, and free at launch like every infrared round. */
inline constexpr FBStoreSpec kMagic1{
    FBStoreKind::Magic1, "magic1", "magic1", 196.2, 0.0896, 60.0,
    /*Guided*/ true, /*RequiresLock*/ true, /*FuzeRadiusM*/ 7.9, /*WarheadKg*/ 12.7,
    FBWeaponPerf{12353.0, 4.0, 0.0, 0.0, 89.0, 67.6, 0.55, 0.019359, 200.0, 0.0, 10000.0, 1.0},
    /*Seeker*/ FBSeekerKind::Infrared, /*FovHalf*/ 5.0, /*GimbalHalf*/ 55.0};

inline constexpr const FBStoreSpec *kStoreCatalogue[] = {&kMk82, &kAim120, &kAim9, &kR73, &kR27r,
                                                         &kV750, &kV601, &k3m9, &k9m33, &kStrela2,
                                                         &kIgla,
                                                         &kAgm88, &kMk84, &kGbu12, &kCbu87,
                                                         &kFab250, &kFab500,
                                                         &kK13, &kR60, &kR24r, &kR40r, &kAim7,
                                                         &kS530f, &kMagic1};

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

/* Append only — the ordinal is the mission-visible `set attack_mode` value and a telemetry column.
 * `Arm` is not a third ballistic solution: it selects which INSTRUMENT supplies the release cue. The
 * two bombing modes read the fire control's countdown; this one reads the warning receiver's bearing,
 * because an anti-radiation shot has no range to count down. doc/air-to-ground.md §7. */
enum class FBDeliveryMode : uint8_t { Ccip = 0, Ccrp, Arm };

inline const char *FBDeliveryModeStr(FBDeliveryMode m) {
  switch (m) {
    case FBDeliveryMode::Ccrp: return "ccrp";
    case FBDeliveryMode::Arm: return "arm";
    case FBDeliveryMode::Ccip: break;
  }
  return "ccip";
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
  /* THE SEEKER CUE of an anti-radiation round, and it is an ANGLE PAIR rather than a position because
   * the shooter's own receiver cannot produce one: a bearing and an elevation off the launcher's nose,
   * plus the class the round was programmed to follow. This is the whole launch programming of that
   * weapon — there is no Target, because nothing measured one. doc/air-to-ground.md §7. */
  bool   CueValid = false;
  double CueAzDeg = 0.0, CueElDeg = 0.0;
  FBArTargetClass ArClass = FBArTargetClass::AnySurface;
};

/* WHAT A DESIGNATOR PUTS ON A POINT — published in the shooter's unit signature beside the midcourse
 * uplink and read by a semi-active LASER round exactly the way that round's radar cousin reads the
 * uplink. One bool, one point, one stamp, and the designator's own id so a bomb takes the spot of the
 * ONE unit that released it. The point is the STEERPOINT: mission-author knowledge, the same class as a
 * `wp` line, and no sensor of any kind. doc/air-to-ground.md §3.2. */
struct FBLaserDesignation {
  bool   Active = false;
  int    DesignatorId = 0;
  double LatDeg = 0.0, LonDeg = 0.0, ElevM = 0.0;
  double StampS = 0.0;
};

} // namespace FlightBox
#endif
