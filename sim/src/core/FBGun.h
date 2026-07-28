/* The GUN catalogue: what an internal gun IS, as data, and what one squeeze of the trigger produces.
 * FBStore.h's sibling, separate because the two weapons differ in KIND: a store becomes its own unit,
 * a gun produces a stream far too numerous to be units at all.
 * THE ONE MODELLING DECISION: a burst IS one ballistic bundle — one launch point, one velocity, one
 * integration, carrying a COUNT and a DISPERSION that enter hit resolution as a DENSITY, never as a
 * position. What is physics, what is modelling and what is deliberately absent (barrel wear, round-to-
 * round spread, ammunition types, ammunition mass): doc/core.md, Abschnitt 7.4. */
#ifndef FBGUN_H
#define FBGUN_H

#include <cstdint>
#include <cstring>

namespace FlightBox {

/* Append only — the ordinal is telemetry-visible. None = this aircraft carries no gun. */
enum class FBGunKind : uint8_t { None = 0, M61A1, Gsh301, Azp23, Zu23 };

struct FBGunSpec {
  FBGunKind Kind = FBGunKind::None;
  const char *Key = "";
  double MuzzleVelMs = 0.0;
  double RoundsPerMin = 0.0;
  int    Capacity = 0;
  double SpoolUpS = 0.0;        /* trigger to full rate; rounds during it come at the ramped rate */
  double RoundMassKg = 0.0;     /* the projectile, not the cartridge — it is what arrives */
  double RoundDiaM = 0.0;       /* reference diameter for the drag area */
  double DragCoef = 0.0;        /* axial drag coefficient on that area */
  double DispersionSigmaRad = 0.0;   /* circular-normal sigma of the round pattern (see kM61A1) */
  double MaxBurstS = 0.0;       /* longest single trigger squeeze the box will honour */
};

/* THE M61A1 VULCAN (doc/modules/f16/weapons.md §2.5, §3, §4.1). RoundMassKg, DragCoef and MaxBurstS are [SET],
 * DispersionSigmaRad is [DERIVED] from the MIL-DTL-45500/1A dispersion citation (circular normal,
 * sigma = 4 mil / sqrt(2*ln5)); source and confidence per number, plus why the ammunition's own MASS is
 * deliberately not modelled: doc/core.md, Abschnitt 7.4. */
inline constexpr FBGunSpec kM61A1{FBGunKind::M61A1, "m61a1",
                                  /*MuzzleVelMs*/ 1030.0, /*RoundsPerMin*/ 6000.0, /*Capacity*/ 510,
                                  /*SpoolUpS*/ 0.3, /*RoundMassKg*/ 0.100, /*RoundDiaM*/ 0.020,
                                  /*DragCoef*/ 0.30, /*DispersionSigmaRad*/ 2.2295e-3,
                                  /*MaxBurstS*/ 1.0};

/* THE GSh-301 (9A-4071K), row for row against the M61A1 above so the two guns are comparable at a
 * glance (doc/modules/mig29/weapons.md §4).
 *   MuzzleVelMs 860        [T3] weaponsystems.net; T4 says 900 — the lower, better-sourced figure is
 *                          taken and the difference recorded there rather than averaged
 *   RoundsPerMin 1500      [DCS-FM p.64] and [DCS-EA p.86]; the hardware range is 1,500-1,800 [T3]
 *   Capacity 150           [DCS-FM p.64], [DCS-EA p.86]
 *   RoundMassKg 0.390      [T4, consistent across sources] — the projectile of the 30x165 mm round
 *   RoundDiaM 0.030        [T3] 30x165 mm
 *   SpoolUpS 0.0           a SINGLE-BARREL, short-recoil gun [DCS-FM p.64] has no rotating mass to spin
 *                          up: the first round leaves on the first cycle. Zero is a statement about
 *                          the mechanism, not a missing number
 *   DragCoef 0.30          [SET], the same slender projectile coefficient the M61A1 row carries — and
 *                          the §4.3 correction table is the acceptance target that says whether it is
 *                          right: 14 Russian thousandths at 1,500 m / 432 kt / 20 deg dive, against
 *                          the 6.0 a drag-free round would need
 *   DispersionSigmaRad     [SET, DERIVED BOUND] — no T1-T3 dispersion figure exists (§8.4). Bounded
 *                          from the documented 800 m effective air-target limit instead of guessed:
 *                          a fighter presents ~10 m of span there, so keeping the bulk of a burst on
 *                          it needs a full cone <= 12.5 mrad, i.e. a half-angle <= 6 mrad. Taken as
 *                          the sigma of the circular-normal pattern, 6.0e-3 rad, which puts 80 % of a
 *                          burst inside 9.0 mrad — the first number to replace when GAF T.O.
 *                          1F-MIG29-1 becomes available
 *   MaxBurstS 1.0          [SET], as the M61A1's: a trigger command is ONE action and needs a duration.
 *                          Here it is 25 rounds rather than 100 — the drum lasts 6.0 s against 5.1 s */
inline constexpr FBGunSpec kGsh301{FBGunKind::Gsh301, "gsh301",
                                   /*MuzzleVelMs*/ 860.0, /*RoundsPerMin*/ 1500.0, /*Capacity*/ 150,
                                   /*SpoolUpS*/ 0.0, /*RoundMassKg*/ 0.390, /*RoundDiaM*/ 0.030,
                                   /*DragCoef*/ 0.30, /*DispersionSigmaRad*/ 6.0e-3,
                                   /*MaxBurstS*/ 1.0};

/* ---- THE TWO GROUND GUNS (doc/modules/ground/catalogue.md) ---------------------------------------
 * Both fire the same 23 x 152B round, so they share every ballistic number and differ only in barrels
 * and feed. They fit the projectile pool by arithmetic and not by luck: FBGunProjectiles retires a
 * bundle at 3 s / 3 000 m, and 970 m/s covers 2 910 m in 3 s against a published 2-2.5 km effective
 * slant range. Every heavier AAA piece (57 mm, 100 mm) is excluded by the same caps, which is why
 * doc/modules/ground/module.md G6 books it as a separate round with the STORE path.
 *
 * RoundMassKg 0.100 [TODO], carried over from the M61A1's own declared [SET] rather than invented a
 * second time — every kinetic damage number hangs on it linearly, and there is one wrong number in the
 * tree instead of two. DispersionSigmaRad likewise [SET] at the M61A1's value: no figure was found for
 * either gun, and two different invented ones would be worse than one shared one. */

/* THE AZP-23 QUAD, the ZSU-23-4's armament. RoundsPerMin 3 400 [T4] — the LOWER bound of the sourced
 * 3,400-4,000 pair, stated. Capacity 2 000 [T4]. MuzzleVelMs 970 [T4]. SpoolUpS 0 [SET]: gas-operated
 * autocannon, no rotating mass. MaxBurstS 1.0 [SET], as every other gun in the tree — a trigger
 * command is ONE action and needs a duration; here that is 57 rounds. */
inline constexpr FBGunSpec kAzp23{FBGunKind::Azp23, "azp23",
                                  /*MuzzleVelMs*/ 970.0, /*RoundsPerMin*/ 3400.0, /*Capacity*/ 2000,
                                  /*SpoolUpS*/ 0.0, /*RoundMassKg*/ 0.100, /*RoundDiaM*/ 0.023,
                                  /*DragCoef*/ 0.30, /*DispersionSigmaRad*/ 2.2295e-3,
                                  /*MaxBurstS*/ 1.0};

/* THE ZU-23-2 TWIN, hand-laid over the ZAP-23 optical sight. RoundsPerMin 800 [DERIVED] = 2 x the
 * sourced 400 rpm PRACTICAL rate: the cyclic 2 000 rpm empties a 50-round belt in 1.5 s, so sustained
 * fire is the practical figure and this gun's whole employment is sustained. Capacity 100 [T4], two
 * 50-round belts. */
inline constexpr FBGunSpec kZu23{FBGunKind::Zu23, "zu23",
                                 /*MuzzleVelMs*/ 970.0, /*RoundsPerMin*/ 800.0, /*Capacity*/ 100,
                                 /*SpoolUpS*/ 0.0, /*RoundMassKg*/ 0.100, /*RoundDiaM*/ 0.023,
                                 /*DragCoef*/ 0.30, /*DispersionSigmaRad*/ 2.2295e-3,
                                 /*MaxBurstS*/ 1.0};

inline constexpr const FBGunSpec *kGunCatalogue[] = {&kM61A1, &kGsh301, &kAzp23, &kZu23};

inline const FBGunSpec *FBFindGun(const char *key) {
  if (!key) return nullptr;
  for (const FBGunSpec *g : kGunCatalogue)
    if (std::strcmp(g->Key, key) == 0) return g;
  return nullptr;
}

inline const FBGunSpec *FBGunSpecOf(FBGunKind kind) {
  for (const FBGunSpec *g : kGunCatalogue)
    if (g->Kind == kind) return g;
  return nullptr;
}

/* ONE bundle of rounds, as the gun hands it over into a queue the OWNER of the simulation drains — the
 * same boundary FBStoreRelease crosses. The velocity is already the SUM of aircraft and muzzle velocity
 * along the bore, so the receiver integrates a plain projectile and needs to know nothing about the
 * aircraft that fired it. */
struct FBGunBurst {
  int    LauncherId = 0;
  FBGunKind Kind = FBGunKind::None;
  int    Rounds = 0;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;
  double VelE = 0.0, VelN = 0.0, VelU = 0.0;
  double SimTimeS = 0.0;
};

} // namespace FlightBox
#endif
