#include "FBF16Damage.h"

namespace FlightBox {

namespace {
/* ---- The four fragility classes (see the header's banner). All [SET], in J/m^2 of fragment energy.
 * For scale, against an AIM-120's 20.5 kg warhead at a head-on closure of ~850 m/s the ranges these
 * correspond to are:
 *   1.2e4  ~ 11.6 m      3.0e4  ~  7.3 m      5.0e4  ~  5.7 m
 *   8.0e4  ~  4.5 m      1.5e5  ~  3.3 m      2.5e5  ~  2.5 m
 * i.e. anything that trips the proximity fuze at all costs avionics, and only a burst inside ~3 m takes
 * the engine or the flight controls with it. ---- */
constexpr double kAvionicsDegrade = 1.2e4;   /* a box: thin skin, no redundancy */
constexpr double kAvionicsFail = 3.0e4;
constexpr double kEngineDegrade = 5.0e4;     /* accessories/nozzle: military power only */
constexpr double kEngineFail = 1.5e5;
constexpr double kFlcsDegrade = 5.0e4;       /* one of two hydraulic systems */
constexpr double kFlcsFail = 1.5e5;
constexpr double kStructDegrade = 8.0e4;     /* skin and stringers: drag */
constexpr double kStructFail = 2.5e5;

/* An avionics system has no derivable degraded behaviour except the radar's (range follows the radar
 * equation, core/FBDamageModel::kRadarRangeDegraded), so every other box declares its degrade threshold
 * equal to its fail threshold and therefore never enters the Degraded state. Modelling "a bit of noise"
 * on an INS or an ADC would be inventing a number, which this file does not do. */
constexpr FBZoneSystem kNoseSystems[] = {
    {FBSystemId::Radar, kAvionicsDegrade, kAvionicsFail},     /* APG-68 antenna + transmitter */
    {FBSystemId::AirData, kAvionicsFail, kAvionicsFail},      /* pitot/AoA probes on the cone */
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};

constexpr FBZoneSystem kForwardSystems[] = {
    {FBSystemId::Nav, kAvionicsFail, kAvionicsFail},          /* INS */
    /* The M61A1 and its drum sit in the left wing-root strake, i.e. in this zone (modules/f16/FBF16Gun).
     * It takes the STRUCTURAL thresholds rather than the avionics ones, and not for effect: a gun is a
     * mechanical installation with the mass and section of the airframe around it, not a black box on a
     * rack, so what it takes to stop it is what it takes to hole the structure it is bolted to. */
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

/* THE PRESENTED AREAS (core/FBDamageModel.h's FBDamageLayout) — what a stream of gunfire sees. Derived
 * from the pinned f16.xml's own geometry rather than picked, and stated as the crude equivalents they
 * are:
 *   Frontal 4.0 m^2   the airframe seen head-on or from astern: a fuselage of roughly 1 m x 1.5 m
 *                     section plus the thin edge of a 27.9 m^2 wing and the fins. It is an EQUIVALENT
 *                     area — no projection of the actual shape is computed anywhere.
 *   Lateral 14 m^2    seen from the side or from above: the model's own <wingarea> is 27.9 m^2 and its
 *                     <wingspan> 9.14 m over a 14.5 m length, so the planform is of that order and the
 *                     side view rather less; 14 m^2 is the middle of the two, which is what a single
 *                     number for "across the axis" can honestly be.
 * Both [SET]. They scale the expected round count linearly, which is why they are named here, once. */
/* ...and how far that material reaches from the centreline in each view (core/FBGunBallistics.h's
 * two-scale hit model): half the model's own <wingspan> (9.14 m -> 4.57 m) seen from astern, and half
 * its own length (14.5 m -> 7.3 m) seen from the side. Both are the model's geometry, not settings. */
constexpr FBDamageLayout kLayout{kZones, Count(kZones), /*FrontalAreaM2*/ 4.0, /*LateralAreaM2*/ 14.0,
                                 /*FrontalExtentM*/ 4.57, /*LateralExtentM*/ 7.3};
} // namespace

const FBDamageLayout &FBF16DamageLayout() { return kLayout; }

} // namespace FlightBox
