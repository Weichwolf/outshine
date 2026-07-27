/* FlightBox — the GROUND-TARGET catalogue: what a thing on the ground IS, as data. One entry per target
 * class, and the same value-type-over-class decision core/FBStore.h makes for a store, for the same
 * reason: a target has no behaviour. It sits where the mission put it, it has an extent, and it has a
 * fragility. Everything that then HAPPENS to it is the ordinary chain every unit goes through — the
 * owner of the simulation resolves a burst against it (app/FBMissionRunner.cpp), core/FBDamageModel
 * decides what the geometry did, and its own units/FBSimUnit health register is where the answer lands.
 *
 * A GROUND TARGET HAS NO FLIGHT DYNAMICS, so it carries no flight dynamics model. That is the whole
 * design decision behind modules/ground/FBGroundModule and it is worth stating here, next to the data:
 * giving a bunker a JSBSim airframe so that it could be spawned by the existing path would have been an
 * invented aerodynamic object, integrated at 100 Hz, to describe something that does not move. Instead
 * the airframe became OPTIONAL at the unit level (units/FBSimUnit) — a unit with one is stepped by
 * JSBSim, a unit without one holds the pose the mission declared — and everything else about a unit is
 * unchanged and shared: identity, team, the published pose, the health register, the damage model, the
 * mission roster, telemetry.
 *
 * WHAT THE FRAGILITY NUMBERS MEAN, and where they come from. Each class declares the areal fragment
 * energy (J/m^2) at which its STRUCTURE degrades and fails — the same currency and the same thresholds
 * mechanism modules/f16/FBF16Damage uses for an airframe, so one damage model answers for both. Only
 * Structure is declared, deliberately: FBSystemHealth::CombatEffective asks about engine, flight
 * controls and structure, and a building has exactly one of the three. Adding a "radar" system to a SAM
 * site would be inventing a consumer that does not exist.
 *
 * The numbers are quoted as the RADII they correspond to for the one weapon that can currently reach
 * them, because that is the only honest way to read a threshold. A Mk-82 (87 kg warhead,
 * core/FBDamageModel's kCaseFraction 0.5 and kFragSpeedMs 1800) arriving at ~245 m/s delivers
 *   flux(r) = 5.71e6 / r^2  J/m^2
 * so the four thresholds below mean:
 *   soft  degrade 1.2e3 ~ 69 m,  fail 2.8e3 ~ 45 m
 *   hard  degrade 2.5e4 ~ 15 m,  fail 9.0e4 ~  8 m
 * All four are [SET]: doc/f16/weapons.md §4.7 lists warhead lethality as a genuine gap and no source in
 * this tree gives a lethal radius. What they are ANCHORED to is the open, widely repeated figure for a
 * 500 lb general-purpose bomb against unprotected targets — an effective/casualty radius of the order of
 * 50-60 m — so 45 m for "finished" and 69 m for "hurt" is a conservative reading of it rather than a
 * generous one. The hard class then says the thing the two classes exist to distinguish: the same weapon
 * needs an effectively direct hit to finish a hardened one. */
#ifndef FBGROUNDTARGET_H
#define FBGROUNDTARGET_H

#include <cstring>
#include "FBDamageModel.h"

namespace FlightBox {

struct FBGroundTargetSpec {
  const char *Key = "";        /* the mission-file `module <name>` / FBModuleRegistry name */
  FBDamageLayout Layout{};
};

namespace GroundTargets {

constexpr FBZoneSystem kSoftSystems[] = {
    {FBSystemId::Structure, /*Degrade*/ 1.2e3, /*Fail*/ 2.8e3},
};
constexpr FBZoneSystem kHardSystems[] = {
    {FBSystemId::Structure, /*Degrade*/ 2.5e4, /*Fail*/ 9.0e4},
};

/* ONE zone, because a target this simulator can currently attack is a point on the ground with an
 * extent, not an articulated object with a nose and a tail. The stretch is its own half-length along
 * its declared heading (core/FBDamageModel measures a burst to the zone's axis segment), so a soft
 * target is a 20 m installation and a hard one a 12 m block. */
constexpr FBDamageZoneSpec kSoftZones[] = {{FBDamageZone::Center, -10.0, 10.0, kSoftSystems, 1}};
constexpr FBDamageZoneSpec kHardZones[] = {{FBDamageZone::Center, -6.0, 6.0, kHardSystems, 1}};

/* Presented area/extent stay ZERO — the FBDamageLayout default that means "presents nothing to a
 * projectile stream", so core/FBGunBallistics' hit model puts no rounds on it. That is not an omission
 * to fix later by guessing: strafing a ground target needs the rounds tracked to the ground, which the
 * gun round did not build (its bundles are resolved against aircraft only), and a presented area here
 * would claim a capability the projectile side does not have. */
inline constexpr FBGroundTargetSpec kSoft{"target_soft", FBDamageLayout{kSoftZones, 1, 0.0, 0.0, 0.0, 0.0}};
inline constexpr FBGroundTargetSpec kHard{"target_hard", FBDamageLayout{kHardZones, 1, 0.0, 0.0, 0.0, 0.0}};

} // namespace GroundTargets

inline constexpr const FBGroundTargetSpec *kGroundTargetCatalogue[] = {&GroundTargets::kSoft,
                                                                      &GroundTargets::kHard};

inline const FBGroundTargetSpec *FBFindGroundTarget(const char *key) {
  if (!key) return nullptr;
  for (const FBGroundTargetSpec *s : kGroundTargetCatalogue)
    if (std::strcmp(s->Key, key) == 0) return s;
  return nullptr;
}

} // namespace FlightBox
#endif
