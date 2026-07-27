/* The GUN catalogue: what an internal gun IS, as data, and what one squeeze of the trigger produces.
 * FBStore.h's sibling, separate because the two weapons differ in KIND: a store becomes its own unit,
 * a gun produces a stream far too numerous to be units at all.
 * THE ONE MODELLING DECISION: a burst IS one ballistic bundle — one launch point, one velocity, one
 * integration, carrying a COUNT and a DISPERSION that enter hit resolution as a DENSITY, never as a
 * position. What is physics, what is modelling and what is deliberately absent (barrel wear, round-to-
 * round spread, ammunition types, ammunition mass): doc/flightbox/core.md, Abschnitt 7.4. */
#ifndef FBGUN_H
#define FBGUN_H

#include <cstdint>
#include <cstring>

namespace FlightBox {

/* Append only — the ordinal is telemetry-visible. None = this aircraft carries no gun. */
enum class FBGunKind : uint8_t { None = 0, M61A1 };

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

/* THE M61A1 VULCAN (doc/f16/weapons.md §2.5, §3, §4.1). RoundMassKg, DragCoef and MaxBurstS are [SET],
 * DispersionSigmaRad is [DERIVED] from the MIL-DTL-45500/1A dispersion citation (circular normal,
 * sigma = 4 mil / sqrt(2*ln5)); source and confidence per number, plus why the ammunition's own MASS is
 * deliberately not modelled: doc/flightbox/core.md, Abschnitt 7.4. */
inline constexpr FBGunSpec kM61A1{FBGunKind::M61A1, "m61a1",
                                  /*MuzzleVelMs*/ 1030.0, /*RoundsPerMin*/ 6000.0, /*Capacity*/ 510,
                                  /*SpoolUpS*/ 0.3, /*RoundMassKg*/ 0.100, /*RoundDiaM*/ 0.020,
                                  /*DragCoef*/ 0.30, /*DispersionSigmaRad*/ 2.2295e-3,
                                  /*MaxBurstS*/ 1.0};

inline constexpr const FBGunSpec *kGunCatalogue[] = {&kM61A1};

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
