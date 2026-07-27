#include "FBF16Damage.h"

namespace FlightBox::Modules {

namespace {
/* ---- THE FOUR FRAGILITY CLASSES: the actual modelling decision of this file. All [SET], in J/m^2 of
 * fragment energy. The ladder is chosen ONCE to read the way a blast-fragmentation warhead behaves
 * against this airframe; every intermediate case then follows from the 1/r^2 law rather than from
 * another number. Scale table: doc/flightbox/modules-f16.md §10.3. ---- */
constexpr double kAvionicsDegrade = 1.2e4;   /* a box: thin skin, no redundancy */
constexpr double kAvionicsFail = 3.0e4;
constexpr double kEngineDegrade = 5.0e4;     /* accessories/nozzle: military power only */
constexpr double kEngineFail = 1.5e5;
constexpr double kFlcsDegrade = 5.0e4;       /* one of two hydraulic systems */
constexpr double kFlcsFail = 1.5e5;
constexpr double kStructDegrade = 8.0e4;     /* skin and stringers: drag */
constexpr double kStructFail = 2.5e5;

/* Only the radar has a DERIVABLE degraded behaviour (range follows the radar equation), so every other
 * box sets degrade = fail and never enters Degraded: "a bit of noise" on an INS would be an invention. */
constexpr FBZoneSystem kNoseSystems[] = {
    {FBSystemId::Radar, kAvionicsDegrade, kAvionicsFail},     /* APG-68 antenna + transmitter */
    {FBSystemId::AirData, kAvionicsFail, kAvionicsFail},      /* pitot/AoA probes on the cone */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

constexpr FBZoneSystem kForwardSystems[] = {
    {FBSystemId::Nav, kAvionicsFail, kAvionicsFail},          /* INS */
    /* STRUCTURAL thresholds, not avionics: a gun is a mechanical installation with the mass and section
     * of the airframe around it, so what stops it is what holes the structure it is bolted to. */
    {FBSystemId::Gun, kStructFail, kStructFail},
    {FBSystemId::FireControl, kAvionicsFail, kAvionicsFail},  /* FCC */
    {FBSystemId::RadarAlt, kAvionicsFail, kAvionicsFail},     /* CARA */
    {FBSystemId::Datalink, kAvionicsFail, kAvionicsFail},     /* MIDS terminal */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

constexpr FBZoneSystem kCenterSystems[] = {
    {FBSystemId::Stores, kAvionicsFail, kAvionicsFail},       /* SMS + station wiring at the wing roots */
    {FBSystemId::FlightControls, kFlcsDegrade, kFlcsFail},    /* hydraulics + actuator runs */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

constexpr FBZoneSystem kAftSystems[] = {
    {FBSystemId::Engine, kEngineDegrade, kEngineFail},
    {FBSystemId::Rwr, kAvionicsFail, kAvionicsFail},          /* ALR-56M aft receivers */
    {FBSystemId::Countermeasures, kAvionicsFail, kAvionicsFail},   /* ALE-47 dispensers */
    {FBSystemId::FlightControls, kFlcsDegrade, kFlcsFail},    /* tail actuators */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

template <typename T, int N>
constexpr int Count(const T (&)[N]) { return N; }

constexpr FBDamageZoneSpec kZones[] = {
    {FBDamageZone::Nose, 3.64, 7.46, kNoseSystems, Count(kNoseSystems)},
    {FBDamageZone::Forward, 0.00, 3.64, kForwardSystems, Count(kForwardSystems)},
    {FBDamageZone::Center, -2.42, 0.00, kCenterSystems, Count(kCenterSystems)},
    {FBDamageZone::Aft, -7.46, -2.42, kAftSystems, Count(kAftSystems)},
};

/* What a stream of gunfire sees. The two AREAS are [SET] EQUIVALENTS derived from the model's own
 * geometry — no projection of the actual shape is computed anywhere — and they scale the expected round
 * count linearly, which is why they are named once, here. The two EXTENTS are the model's geometry
 * outright: half its <wingspan> from astern, half its length from the side. §10.4. */
constexpr FBDamageLayout kLayout{kZones, Count(kZones), /*FrontalAreaM2*/ 4.0, /*LateralAreaM2*/ 14.0,
                                 /*FrontalExtentM*/ 4.57, /*LateralExtentM*/ 7.3};
} // namespace

const FBDamageLayout &FBF16DamageLayout() { return kLayout; }

} // namespace FlightBox::Modules
