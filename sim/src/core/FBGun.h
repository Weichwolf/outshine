/* FlightBox — the GUN catalogue: what an internal gun IS, as data, and what one squeeze of the trigger
 * produces. core/FBStore.h's sibling, and deliberately a SEPARATE file for the reason the two weapons
 * differ in kind: a store is an object that hangs on a pylon and becomes its own JSBSim unit the moment
 * it is released; a gun is a fixed installation that never leaves the aircraft and whose product is a
 * stream of rounds far too numerous to be units at all (6,000 rd/min against a 0.1 s tick is ten rounds
 * per tick, per firing aircraft).
 *
 * THE ONE MODELLING DECISION, stated once here and honoured everywhere below: a BURST IS ONE BALLISTIC
 * BUNDLE. Every round a tick's worth of trigger produces shares one launch point, one launch velocity
 * and one integration (core/FBGunProjectiles); what makes it a burst rather than a single round is that
 * it carries a COUNT and a DISPERSION ANGLE, and that both enter the hit resolution as a density rather
 * than as a position. Nothing about a round's own position is claimed, because nothing here knows it.
 *
 * WHAT IS PHYSICS AND WHAT IS MODELLING — the whole point of the split, so that no number below can be
 * mistaken for a measurement:
 *   PHYSICS (integrated, not tabled): the bundle's trajectory. Muzzle velocity adds to the aircraft's
 *     own velocity vector, gravity acts, and quadratic drag decelerates it against the ISA density at
 *     its own altitude (core/FBGunBallistics.h). Time of flight, drop and impact speed are therefore
 *     computed, and the fire control's lead solution is a solve against that same trajectory rather
 *     than a lookup.
 *   MODELLING: (a) that one bundle stands for N rounds, (b) that the rounds are distributed inside the
 *     bundle as a circular normal about its axis, and (c) that a hit is an EXPECTED number of rounds
 *     and an areal energy density rather than a set of individual impacts. (b) is fitted to the one
 *     dispersion figure the source material states (see kM61A1); (c) is what makes the model
 *     deterministic — there is no random number anywhere in the gun path, so a burst fired from the
 *     same geometry always does the same damage, exactly as core/FBDamageModel promises for a warhead.
 *   NEITHER, and stated as absent: barrel wear, round-to-round velocity spread, tracer/HEI/API mix
 *     (weapons.md §3 lists six ammunition types; the drum here is one homogeneous round), and the mass
 *     of the ammunition itself — see kM61A1's DrumMass note. */
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

/* THE M61A1 VULCAN (doc/f16/weapons.md §2.5, §3, §4.1). Sources and confidence, per number:
 *   MuzzleVelMs 1030    3,380 ft/s for standard rounds [T4, §4.1 — "consistent across sources, no
 *                       T1/T2 found"]. PGU-28/B is 20 m/s faster; one round type is modelled.
 *   RoundsPerMin 6000   [ED figure, §2.5 and §3 agree].
 *   Capacity 510        [ED §3's drum figure. §2.5 quotes 512 in the same guide; the two differ by two
 *                       rounds and §3 is the specification table, so §3 wins and the discrepancy is
 *                       noted rather than averaged.]
 *   SpoolUpS 0.3        [T4, §4.1 — flagged there as needing a T1/T2 citation. It is modelled because
 *                       omitting it would silently claim instantaneous full rate, which is the larger
 *                       error: at 6,000 rd/min it is worth ~15 rounds per squeeze.]
 *   RoundMassKg 0.100   [SET — and this is the one number §4.1 explicitly refuses to certify: "do not
 *                       treat the ED dispersion footnote's projectile mass as authoritative spec data".
 *                       FlightBox needs a mass to turn a hit into energy, so it uses the ~100 g class as
 *                       a stated SETTING. Every kinetic damage figure in this simulator is linear in it,
 *                       which is exactly why it is named here and nowhere else.]
 *   RoundDiaM 0.020     20x102 mm [ED §3] — the calibre, hence the drag reference area.
 *   DragCoef 0.30       [SET] for a spin-stabilised supersonic projectile. It is not a citation, and it
 *                       is checkable rather than free: with the mass and calibre above it puts the time
 *                       of flight to 1,000 m at ~1.3 s (make test-gun prints it), which is the order
 *                       every published 20 mm firing table shows.
 *   DispersionSigmaRad  [DERIVED from the one dispersion specification the guides carry, §2.5's
 *                       MIL-DTL-45500/1A citation: "80% of a 75-round burst within an 8.0 in circle at
 *                       1,000 in", i.e. 80% inside a 4 mil RADIUS. For a circular-normal pattern
 *                       P(r<R) = 1 - exp(-R^2/2s^2), so s = 4 mil / sqrt(2*ln5) = 2.2295 mil. The fit is
 *                       checkable against the SECOND figure in the same citation, which was not used to
 *                       make it: it predicts 97.3% inside the 12 mil (6 mil radius) circle the guide
 *                       calls 100%. A uniform disc — the obvious alternative — would have put only 44%
 *                       inside the 8 mil circle and is therefore ruled out by the source, not by taste.]
 *   MaxBurstS 1.0       [SET] — the longest squeeze the gun honours in one command, i.e. 100 rounds.
 *                       A trigger command is one pilot ACTION (core/FBAvionicsCommand.h), so it needs a
 *                       duration; this is the ceiling, not a doctrine.
 * DELIBERATELY ABSENT — the ammunition's MASS. 510 rounds are of the order of 110 lb, and firing them
 * off would move the aircraft's weight and CG. It is not modelled because the vanilla f16.xml's empty
 * weight cannot be decomposed (CLAUDE.md Prinzip 1: the model is read-only and its mass breakdown is
 * its own), so adding a drum as a point mass would as likely double-count it as correct it. The
 * omission is under half a percent of gross weight and is stated instead of hidden. */
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

/* ONE BUNDLE OF ROUNDS, as the gun hands it over: where it left the aircraft, how fast and in which
 * direction, how many rounds it stands for, and who fired it. The gun system produces these in a queue
 * and the OWNER of the simulation drains it — the same boundary FBStoreRelease crosses, for the same
 * reason: what a projectile does to another unit is resolved on the published poses by the client, never
 * by the system that fired it (CLAUDE.md "Kein Cheaten").
 *
 * The velocity is already the SUM of the aircraft's own velocity and the muzzle velocity along the bore
 * (that sum is physics and the gun knows both halves), so the receiver integrates a plain projectile and
 * needs to know nothing about the aircraft that fired it. */
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
