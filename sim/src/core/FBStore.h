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
enum class FBStoreKind : uint8_t { None = 0, Mk82, Aim120, Aim9, R73, R27r };

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
 * Append only — telemetry-visible through the round's own trace. doc/weapons.md, Spec. */
enum class FBSeekerKind : uint8_t { None = 0, ActiveRadar, Infrared, SemiActiveRadar };

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
 *                        impact, and the Support state falls back to the predicted time of flight. */
inline double FBSeekerHandoverS(FBSeekerKind k, double timeToActivationS) {
  switch (k) {
    case FBSeekerKind::Infrared: return 0.0;
    case FBSeekerKind::ActiveRadar: return timeToActivationS;
    case FBSeekerKind::SemiActiveRadar:
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

inline constexpr const FBStoreSpec *kStoreCatalogue[] = {&kMk82, &kAim120, &kAim9, &kR73, &kR27r};

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
};

} // namespace FlightBox
#endif
