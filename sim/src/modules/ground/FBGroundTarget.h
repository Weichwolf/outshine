/* FlightBox — the GROUND-TARGET catalogue: what a thing on the ground IS, as data, exactly the
 * value-type-over-class decision core/FBStore.h makes for a store and for the same reason — a target has
 * no behaviour. It sits where the mission put it, it has an extent, and it has a fragility; everything
 * that then HAPPENS to it is the ordinary chain every unit goes through.
 *
 * Only STRUCTURE is declared, deliberately: CombatEffective asks about engine, flight controls and
 * structure, and a building has exactly one of the three — giving a SAM site a "radar" system would be
 * inventing a consumer that does not exist. The four thresholds are [SET] (no source in this tree gives
 * a lethal radius), ANCHORED to the open figure for a 500 lb bomb against unprotected targets; the radii
 * they correspond to, and the whole derivation, are doc/flightbox/weapons-and-damage.md §9. */
#ifndef FBGROUNDTARGET_H
#define FBGROUNDTARGET_H

#include <cstring>
#include "FBDamageModel.h"

namespace FlightBox::Modules {

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

/* ONE zone: a target this simulator can attack is a point on the ground with an extent, not an
 * articulated object with a nose and a tail. The stretch is its half-length along its declared heading:
 * a 20 m installation and a 12 m block. */
constexpr FBDamageZoneSpec kSoftZones[] = {{FBDamageZone::Center, -10.0, 10.0, kSoftSystems, 1}};
constexpr FBDamageZoneSpec kHardZones[] = {{FBDamageZone::Center, -6.0, 6.0, kHardSystems, 1}};

/* Presented area/extent stay ZERO = "presents nothing to a projectile stream". Not an omission to fix
 * by guessing: gun bundles are resolved against aircraft only, so an area here would claim a capability
 * the projectile side does not have. */
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

} // namespace FlightBox::Modules
#endif
