#include "FBMig29Damage.h"

namespace FlightBox::Modules {

namespace {
/* ---- THE FOUR FRAGILITY CLASSES, in J/m^2 of fragment energy. [SET], and TAKEN OVER VERBATIM from
 * modules/f16/FBF16Damage.cpp rather than re-chosen: the ladder describes what a blast-fragmentation
 * warhead does to a fighter-sized aluminium airframe, and nothing in doc/mig29/ distinguishes this
 * one's skin, hydraulics or engine accessories from the F-16's at that resolution. Inventing a second
 * ladder would express a difference nobody measured. ---- */
constexpr double kAvionicsDegrade = 1.2e4;
constexpr double kAvionicsFail = 3.0e4;
constexpr double kEngineDegrade = 5.0e4;
constexpr double kEngineFail = 1.5e5;
constexpr double kFlcsDegrade = 5.0e4;
constexpr double kFlcsFail = 1.5e5;
constexpr double kStructDegrade = 8.0e4;
constexpr double kStructFail = 2.5e5;

/* [GAP] THE TWIN-ENGINE CASE IS NOT MODELLED, and is flagged rather than fudged: core/FBSystemHealth
 * carries ONE FBSystemId::Engine, so "one RD-33 out, one running" — the single most characteristic
 * damage state of this airframe — has no state to live in. Halving the thresholds to fake redundancy
 * would be a number pretending to be physics. The engine zone therefore reads as the F-16's does. */

constexpr FBZoneSystem kNoseSystems[] = {
    {FBSystemId::Radar, kAvionicsDegrade, kAvionicsFail},     /* N019 antenna + transmitter in the radome */
    {FBSystemId::AirData, kAvionicsFail, kAvionicsFail},      /* pitot boom + AoA vanes on the cone */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

constexpr FBZoneSystem kForwardSystems[] = {
    {FBSystemId::Nav, kAvionicsFail, kAvionicsFail},
    /* STRUCTURAL thresholds for the gun, same reasoning as the F-16: the GSh-301 is a mechanical
     * installation, and what stops it is what holes the port LERX root it is bolted into. */
    {FBSystemId::Gun, kStructFail, kStructFail},
    {FBSystemId::FireControl, kAvionicsFail, kAvionicsFail},
    {FBSystemId::RadarAlt, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Datalink, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

constexpr FBZoneSystem kCenterSystems[] = {
    {FBSystemId::Stores, kAvionicsFail, kAvionicsFail},       /* pylon wiring at the wing roots */
    {FBSystemId::FlightControls, kFlcsDegrade, kFlcsFail},    /* hydraulics + the ARU/SOS linkage runs */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

constexpr FBZoneSystem kAftSystems[] = {
    {FBSystemId::Engine, kEngineDegrade, kEngineFail},
    {FBSystemId::Rwr, kAvionicsFail, kAvionicsFail},          /* SPO-15 aft receivers */
    {FBSystemId::Countermeasures, kAvionicsFail, kAvionicsFail},
    {FBSystemId::FlightControls, kFlcsDegrade, kFlcsFail},    /* stabilator + rudder actuators */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

template <typename T, int N>
constexpr int Count(const T (&)[N]) { return N; }

/* ---- THE BOUNDARIES, each one a station the deck itself states, converted to metres FORWARD of the
 * CG (the layout's convention). CG = 366 in aft of the nose tip <mass_balance>; 1 in = 0.0254 m.
 *
 *   +9.30 m  NOSE_TIP structure contact, x = 0 in           -> the front end
 *   +4.70 m  EYEPOINT, x = 181 in <metrics>                 -> cockpit sill: radome/avionics ends here
 *    0.00 m  CG, x = 366 in
 *   -1.63 m  tanks 3a/3b, x = 430 in, the deck's own note "flanking the ENGINE BAY" -> the bay's front
 *   -8.03 m  aft end of the 682 in airframe [DCS-FM p.14]   -> the tail
 *
 * The Nose/Forward cut is the F-16 layout's cut made from this aircraft's own EYEPOINT; the
 * Center/Aft cut is NOT (the F-16 uses its ventral fins, which this aircraft does not have) — on a
 * twin-engine airframe the honest boundary is where the engine bay starts, and the deck names it. ---- */
constexpr FBDamageZoneSpec kZones[] = {
    {FBDamageZone::Nose, 4.70, 9.30, kNoseSystems, Count(kNoseSystems)},
    {FBDamageZone::Forward, 0.00, 4.70, kForwardSystems, Count(kForwardSystems)},
    {FBDamageZone::Center, -1.63, 0.00, kCenterSystems, Count(kCenterSystems)},
    {FBDamageZone::Aft, -8.03, -1.63, kAftSystems, Count(kAftSystems)},
};

/* The two EXTENTS are the deck's geometry outright: half of <wingspan> 11.36 m from astern, half of the
 * documented 17.32 m length from the side. The two AREAS are the F-16's declared [SET] equivalents
 * scaled by ONE linear factor built from this aircraft's own two dimensions —
 * k = sqrt((11.36/9.14) * (17.32/14.60)) = 1.214, k^2 = 1.474 — so 4.0 -> 5.90 and 14.0 -> 20.64. One
 * formula, both areas, and it says only what it can: this airframe is ~21 % larger in linear size. */
constexpr FBDamageLayout kLayout{kZones, Count(kZones), /*FrontalAreaM2*/ 5.90, /*LateralAreaM2*/ 20.64,
                                 /*FrontalExtentM*/ 5.68, /*LateralExtentM*/ 8.66};
} // namespace

const FBDamageLayout &FBMig29DamageLayout() { return kLayout; }

} // namespace FlightBox::Modules
