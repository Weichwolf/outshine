/* The STORE catalogue: what an external store IS, as data — mass, drag, a model name.
 * Every number is the store's own JSBSim model or derived from it by a stated formula;
 * derivations: doc/flightbox/core.md, Abschnitt 7.1. */
#ifndef FBSTORE_H
#define FBSTORE_H

#include <cstdint>
#include <cstring>
#include "FBWeaponUplink.h"

namespace FlightBox {

/* Append only — the ordinal is telemetry-visible; None must stay 0 (a zeroed block = "nothing loaded"). */
enum class FBStoreKind : uint8_t { None = 0, Mk82, Aim120 };

/* The FIRE-CONTROL COMPUTER'S performance table for a round — deliberately a coarser copy of what the
 * weapon's JSBSim model does, so the prediction error stays measurable rather than tuned away.
 * An unguided store uses the same table and only its four falling-body entries.
 * doc/flightbox/core.md, Abschnitt 7.1. */
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
};

/* Mk-82, 500 lb GP bomb (doc/f16/weapons.md §3). Perf.ArmingS 2.0 [SET], WarheadKg [T3], and
 * Perf.DragCoefA 0.142 [DERIVED, deliberately coarse — the prediction error is the measured thing];
 * every figure derived in doc/flightbox/core.md, Abschnitt 7.1. */
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

/* AIM-120 AMRAAM (doc/f16/weapons.md §2.5, §3, §4.4); FlightBox-own model, flown by modules/missile.
 * FuzeRadiusM/MaxFlightS/MinSpeedMs/ActivationRangeM/SeekerRangeM/ArmingS are [SET] — the published
 * figures are a genuine gap; MassLbs/WarheadKg [T3], DragAreaFt2 [DERIVED].
 * Derivations: doc/flightbox/core.md, Abschnitt 7.1. */
inline constexpr FBStoreSpec kAim120{
    FBStoreKind::Aim120, "aim120", "aim120", 335.0, 0.115, 120.0,
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

/* Append only — the ordinal is the mission-visible `set attack_mode` value and a telemetry column. */
enum class FBDeliveryMode : uint8_t { Ccip = 0, Ccrp };

inline const char *FBDeliveryModeStr(FBDeliveryMode m) {
  return m == FBDeliveryMode::Ccrp ? "ccrp" : "ccip";
}

/* What an unguided release was aimed with — a RECORD, nothing here steers: the prediction leaves the
 * jet with the weapon so the measured impact can be put beside it (doc/flightbox/core.md, 7.2). */
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
