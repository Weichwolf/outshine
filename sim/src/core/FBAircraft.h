/* The CATALOGUE AIRCRAFT: what a flown airframe FlightBox is NOT judged on IS, as data. The
 * value-type-over-class decision core/FBSite.h, core/FBStore.h and core/FBGun.h already make, for the
 * same reason: eighteen aircraft, one behaviour. Every number's source and confidence tier is
 * doc/modules/air/catalogue.md; only what a number MEANS to the code is repeated here.
 *
 * THE THIRD LEVEL (doc/modules/air/module.md §Spec 1). A MODULE is a flown airframe FlightBox is judged
 * on — a pinned or anchor-measured deck, ~20 classes and a reference base directory of its own; two
 * exist. A CATALOGUE CELL is a flown airframe FlightBox is not judged on — one parametric class, one
 * row here, a GENERATED deck or no deck at all. A UNIT is everything perceived and shot at without
 * being flown.
 *
 * TWO MOTION LAWS AND NO ROW GETS BOTH: a row flies on JSBSim iff its own manoeuvre decides an outcome
 * AND its envelope is published. Those two are the same fact seen twice — fighter data IS envelope
 * data — so the test never splits a row. Ten decks, eight movers. */
#ifndef FBAIRCRAFT_H
#define FBAIRCRAFT_H

#include <cstring>
#include "FBDamageModel.h"
#include "FBGun.h"

namespace FlightBox {

/* Ordinals are telemetry-visible (`air_tier`) — append, never reorder. A tier is not a class: it is
 * (a) which `set task` values the module accepts, (b) which sensor slots it powers, and (c) which of
 * its own MEASURED hooks it can fill. doc/modules/air/module.md §Spec 5. */
enum class FBAirTier : uint8_t {
  Track = 0,      /* T0: waypoints or an orbit, the eye, no weapon, reacts to nothing */
  Reflex,         /* T1: T0 plus ONE state — drag on its own RWR's report of a fire-control threat */
  Visual,         /* T2: the Bfm phase with NO radar picture — it fights what it can see */
  Gci,            /* T3: the seven-state intercept machine WITHOUT the cooperative half */
  Peer,           /* T4: everything the F-16 accepts, formation included */
};

inline const char *FBAirTierStr(FBAirTier t) {
  switch (t) {
    case FBAirTier::Track: return "T0";
    case FBAirTier::Reflex: return "T1";
    case FBAirTier::Visual: return "T2";
    case FBAirTier::Gci: return "T3";
    case FBAirTier::Peer: return "T4";
  }
  return "?";
}

/* THE ACQUISITION SET. RangeM 0 = this row has no radar at all and acquires through the eye — which
 * doc/modules/ground/catalogue.md's `zu23` row already proved is a DESIGN and not a hole. */
struct FBAirRadarSpec {
  double SearchRangeM = 0.0;
  double TrackRangeM = 0.0;
  double AzHalfDeg = 60.0;        /* the tree's own fighter default where a row publishes no field */
  double ElCenterDeg = 0.0;
  double ElHalfDeg = 10.5;
  /* FrameS = 4.0 s x (AzHalfDeg / 60 deg) — the tree's OWN declared relation (doc/sensors.md §4.2:
   * "this relation, not the absolute seconds, is the model"), because no published source gives a scan
   * period for any airborne fire-control radar in this catalogue. The one exception is the E-3's
   * rotodome, whose 6 rpm is sourced and whose 10.0 s is where the relation gets CHECKED. */
  double FrameS = 4.0;
  /* THE LOOK-DOWN GATE, and on two rows it is the whole aircraft. 0 = the set cannot look down at all
   * (the MiG-21's RP-22: "it couldn't intercept targets flying under the MiG"); positive = the reduced
   * range it manages against a target BELOW the antenna's own horizon. Negative sentinel -1 = no
   * limit, which is every western set and the Cyrano IV. */
  double LookDownRangeM = -1.0;
  double DopplerNotchMs = 0.0;    /* 0 = no clutter gate: a pulse set cannot be chaffed at all */
  bool   NotchRejects = false;
};

/* THE AIRFRAME NUMBERS pilot/FBPilot reads as virtual hooks. The last two are the TIER GATE: a row
 * whose roll plant is unmeasured is T0 or T1 and the boot REFUSES `set task bfm` with SET_REJECTED,
 * because doc/pilot.md's close-combat law INVERTS the plant and with another aircraft's it is not a
 * limiter but an oscillator (measured: the MiG-29 rolls 2.6x harder for the same stick). */
struct FBAirPerf {
  double CornerKt = 0.0;
  double CornerG = 0.0;
  double MaxG = 0.0;              /* A5, the published g limit; 0 = [TODO] and the limiter is [SET] */
  double AlphaLimitDeg = 0.0;
  double MinSpeedKt = 0.0;
  double ClimbSpeedKt = 0.0;
  double ApproachKt = 0.0;
  /* systems/FBFlightControl's pitch authority cap [DERIVED], doc/modules/air/flight-model-recipe.md
   * §6.1: the stick fraction at which full travel TRIMS 1.5x this row's own alpha limit, i.e.
   * 1.5*|Cma|*a_lim/(|Cmde|*de_max) on its own generated deck. It used to be one [SET] 0.85 on nine
   * rows and 0.70 on the tenth, and nothing read it — FBAirModule flew every row on the MiG-29's
   * preset, which cost air-bomber-intercept.fbm 8 000 m of altitude in 242 s. */
  double PitchStickMax = 0.0;
  double RollPlantA = 0.0;        /* 0 = UNMEASURED. Recipe step 7 has not been run on this row. */
  double RollPlantKDegS = 0.0;
};

/* THE MOVER'S WHOLE STATE LAW. Position, altitude, heading, speed and nothing else; a great-circle leg
 * at the declared speed, with a turn radius from that speed and a [SET] 25 deg bank so a track has a
 * curve and an intercept has a geometry. Never shown to core/FBFlightMonitor (no airframe), never given
 * a control channel, never given a trim state. */
struct FBAirMoverSpec {
  double CruiseMs = 0.0;
  double MaxMs = 0.0;
  double CeilingM = 0.0;
  double ClimbMs = 0.0;
  double BankDeg = 25.0;          /* [SET]: a transport's standard rate turn, and the only shape input */
};

struct FBAircraftSpec {
  const char *Key = "";           /* the mission-file `module <name>` / FBModuleRegistry name */
  const char *Name = "";
  FBAirTier Tier = FBAirTier::Track;
  /* THE MOTION LAW, said by ONE field: a JSBSim model name, or empty for a kinematic mover. It is the
   * same signal units/FBSimUnit already reads to decide whether to build an FBFdm at all, so the two
   * motion laws cost no new mechanism. */
  const char *FdmModel = "";
  int Engines = 1;

  FBAirRadarSpec Radar{};
  bool   HasRwr = false;
  double IrstRangeM = 0.0;        /* 0 = no infrared search-and-track head */
  double IrstHighRangeM = 0.0;
  /* AN EARLY-WARNING NODE, and this is the file's most dangerous bool. True = the row publishes an
   * FBNetReport built from its OWN anonymous radar contact: a POINT with the node's own look age, no id
   * field, no team field. It moves an ANTENNA. It never creates a TRACK. */
  bool   NetNode = false;
  /* ...and the other end: a row that SUBSCRIBES to such a feed. It gets sensors/FBNetLinkSystem, whose
   * own FBState block exists precisely so a controller cue cannot overwrite the Link-16 PPLI a
   * cooperative flight reads (doc/air-defence-network.md §2 design B, made due here). */
  bool   NetMember = false;

  int    Stations = 0;            /* pylons a `store` line may load; 0 = this row carries nothing */
  FBGunKind Gun = FBGunKind::None;
  int    GunRounds = 0;

  FBAirPerf Perf{};
  FBAirMoverSpec Mover{};

  /* 0 = NOT DECLARED, for all eighteen rows and deliberately (catalogue A3): the tree holds exactly two
   * cross-sections measured against each other, and the sigma^(1/4) law makes a wrong value cheap to
   * spot — which is exactly why inventing sixteen is worse than declaring none. The radar then reads
   * "no scaling", i.e. the F-16 reference range. */
  double RcsM2 = 0.0;

  FBDamageLayout Layout{};

  bool IsMover() const { return FdmModel[0] == '\0'; }
};

namespace Aircraft {

/* ---- THE FRAGILITY LADDER, in J/m^2, TAKEN OVER VERBATIM from modules/f16/FBF16Damage.cpp and
 * modules/mig29/FBMig29Damage.cpp rather than re-chosen. It describes what a blast-fragmentation
 * warhead does to a fighter-sized aluminium airframe, and nothing in doc/modules/air/ distinguishes a
 * MiG-21's skin from an F-16's at that resolution. Inventing a third ladder would express a difference
 * nobody measured. ---- */
constexpr double kAvionicsDegrade = 1.2e4, kAvionicsFail = 3.0e4;
constexpr double kEngineDegrade = 5.0e4, kEngineFail = 1.5e5;
constexpr double kFlcsDegrade = 5.0e4, kFlcsFail = 1.5e5;
constexpr double kStructDegrade = 8.0e4, kStructFail = 2.5e5;

/* FOUR ZONES, and the SYSTEM CONTENT of each is shared by every deck row because six of the fourteen
 * existing system ids carry everything a catalogue aircraft has (module.md §Spec 8). What is per-row is
 * only WHERE the zones sit, which is that row's own length. Engine2 is declared in the aft zone of
 * every row and is simply never failed on a single-engine one — the id already exists and a twin that
 * loses one is a different aircraft. */
constexpr FBZoneSystem kNoseSystems[] = {
    {FBSystemId::Radar, kAvionicsDegrade, kAvionicsFail},
    {FBSystemId::AirData, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
constexpr FBZoneSystem kForwardSystems[] = {
    {FBSystemId::Gun, kStructFail, kStructFail},
    {FBSystemId::FireControl, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Datalink, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
constexpr FBZoneSystem kCenterSystems[] = {
    {FBSystemId::Stores, kAvionicsFail, kAvionicsFail},
    {FBSystemId::FlightControls, kFlcsDegrade, kFlcsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
constexpr FBZoneSystem kAftSystems[] = {
    {FBSystemId::Engine, kEngineDegrade, kEngineFail},
    {FBSystemId::Engine2, kEngineDegrade, kEngineFail},
    {FBSystemId::Rwr, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Countermeasures, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
/* A MOVER DECLARES ONE ZONE AND `Structure` ALONE — exactly what `target_hard` declares and for the
 * same reason: nothing else about it has a behaviour to lose. */
constexpr FBZoneSystem kHullSystems[] = {{FBSystemId::Structure, kStructDegrade, kStructFail}};

/* The zone boundaries are metres FORWARD of the CG, and the CG is the deck's own station at half the
 * row's length: nose tip at +L/2, tail at -L/2, with the cockpit sill at +0.24 L and the engine bay
 * front at -0.16 L. Presented AREA and EXTENT are the published span and length — half span frontal,
 * half length lateral — which is also what feeds the EYE (units/FBSimUnit publishes 2x each into
 * FBVisualSignature). Span and length are the ONE sensor input this catalogue can source for every row
 * without exception, which is why the eye is its best-modelled channel. */
#define FB_AIR_ZONES(key, L)                                                       \
  constexpr FBDamageZoneSpec k##key##Zones[] = {                                   \
      {FBDamageZone::Nose, 0.24 * (L), 0.50 * (L), kNoseSystems, 3},               \
      {FBDamageZone::Forward, 0.0, 0.24 * (L), kForwardSystems, 4},                \
      {FBDamageZone::Center, -0.16 * (L), 0.0, kCenterSystems, 3},                 \
      {FBDamageZone::Aft, -0.50 * (L), -0.16 * (L), kAftSystems, 5}};
#define FB_MOVER_ZONES(key, L) \
  constexpr FBDamageZoneSpec k##key##Zones[] = {                                   \
      {FBDamageZone::Center, -0.50 * (L), 0.50 * (L), kHullSystems, 1}};
/* Frontal area ~ 0.06 x span x length and lateral ~ 0.35 x span x length [SET], the same two shape
 * factors for every row: a gun burst's presented-area law needs a number, and one declared pair of
 * factors applied to published dimensions is honester than eighteen invented areas. */
#define FB_AIR_LAYOUT(key, span, len) \
  FBDamageLayout { k##key##Zones, 4, 0.06 * (span) * (len), 0.35 * (span) * (len), \
                   0.5 * (span), 0.5 * (len), 0.5 * (len) }
#define FB_MOVER_LAYOUT(key, span, len) \
  FBDamageLayout { k##key##Zones, 1, 0.06 * (span) * (len), 0.35 * (span) * (len), \
                   0.5 * (span), 0.5 * (len), 0.5 * (len) }

FB_AIR_ZONES(F15c, 19.43)
FB_AIR_ZONES(Su27, 21.9)
FB_AIR_ZONES(Mig21, 14.7)
FB_AIR_ZONES(Mig23, 16.7)
FB_AIR_ZONES(Mig25, 23.82)
FB_AIR_ZONES(Mig17, 11.264)
FB_AIR_ZONES(Su7, 16.8)
FB_AIR_ZONES(Su22, 19.02)
FB_AIR_ZONES(Mirf1, 15.3)
FB_AIR_ZONES(F5e, 14.69)
FB_MOVER_ZONES(E3, 46.61)
FB_MOVER_ZONES(E2c, 17.6)
FB_MOVER_ZONES(Kc135, 41.53)
FB_MOVER_ZONES(Tu95, 46.2)
FB_MOVER_ZONES(An26, 23.8)
FB_MOVER_ZONES(Ef111, 23.16)
FB_MOVER_ZONES(Mi8, 18.17)
FB_MOVER_ZONES(Ah64, 17.73)

/* ============================ THE TEN DECK ROWS ============================
 * Each flies a deck GENERATED by tools/gen_air_decks.py from its eight published anchors. The Perf
 * hooks below are the recipe's step 7 — MEASURED on the generated deck by `make -C sim test-air`, not
 * set — and a row whose roll plant is still zero has not been through it. */

/* F-15C: the peer western opponent, four campaigns. T4, and the row that makes the SARH/active pairing
 * interesting on ONE aircraft — with Sparrows it fights the MiG-29's bound fight, with AMRAAMs the
 * F-16's. Radar 160 km [T4] against the source's unstated reference target; the direction of that error
 * is stated in the catalogue rather than corrected here. */
inline constexpr FBAircraftSpec kF15c{
    "f15c", "F-15C Eagle", FBAirTier::Peer, "f15c", 2,
    FBAirRadarSpec{160000.0, 80000.0, 60.0, 0.0, 10.5, 4.0, -1.0, 40.0, false},
    /*HasRwr*/ true, /*Irst*/ 0.0, 0.0, /*NetNode*/ false, /*NetMember*/ true,
    /*Stations*/ 8, FBGunKind::M61A1, 940,
    FBAirPerf{330.0, 7.0, 9.0, 22.0, 200.0, 350.0, 150.0, 0.273, 0.0, 0.0},
    FBAirMoverSpec{}, /*RcsM2*/ 0.0, FB_AIR_LAYOUT(F15c, 13.05, 19.43)};

/* Su-27S: NO CAMPAIGN FILE NAMES THIS TYPE (catalogue A11) — it is in the catalogue because the round
 * asked for it, and that is stated rather than dressed up. The one row whose published radar ranges
 * state their reference RCS (3 m^2), so the sigma^(1/4) conversion to the tree's 1.2 m^2 reference can
 * be DONE rather than assumed: 100 -> 79.5 km search, 65 -> 51.7 km track. */
inline constexpr FBAircraftSpec kSu27{
    "su27", "Su-27S", FBAirTier::Peer, "su27", 2,
    FBAirRadarSpec{79500.0, 51700.0, 60.0, 0.0, 10.5, 4.0, -1.0, 40.0, false},
    /*HasRwr*/ true, /*Irst*/ 50000.0, 70000.0, false, true,
    /*Stations*/ 10, FBGunKind::Gsh301, 150,
    FBAirPerf{340.0, 7.0, 9.0, 24.0, 210.0, 360.0, 160.0, 0.278, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Su27, 14.7, 21.9)};

/* MiG-21bis: three campaigns, and the row that is NOT a weak MiG-29. Its +-30 x +-10 deg field is a
 * QUARTER of the sky an F-16's CRM covers, it cannot look down AT ALL, and its longest-ranged weapon is
 * an 8 km infrared round — an aircraft that can only be brought to a fight by somebody else, which is
 * what the source says about its doctrine and what `set brief_gci` already expresses.
 * FrameS 2.0 s [DERIVED] by the tree's own relation from the sourced +-30 deg: it sweeps its narrow
 * field twice as fast as an F-16 sweeps its wide one, and sees a quarter as much sky. */
inline constexpr FBAircraftSpec kMig21{
    "mig21", "MiG-21bis", FBAirTier::Gci, "mig21", 1,
    FBAirRadarSpec{20000.0, 10000.0, 30.0, 0.0, 10.0, 2.0, /*LookDown*/ 0.0, 0.0, false},
    /*HasRwr*/ true, 0.0, 0.0, false, true,
    /*Stations*/ 4, FBGunKind::Gsh23l, 200,
    FBAirPerf{300.0, 6.0, 8.5, 20.0, 190.0, 330.0, 145.0, 0.203, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Mig21, 7.154, 14.7)};

/* MiG-23MLD, the N003E export set [SET] because O1's anchor is the Syrian force and the source names
 * that set for exactly those aircraft. The low-altitude number IS the row: 14 km head-on look-down
 * against the D-III's "only above 1 000 m", and the difference between those two sets is the difference
 * between a strike package that can go under the radar and one that cannot. */
inline constexpr FBAircraftSpec kMig23{
    "mig23", "MiG-23MLD", FBAirTier::Gci, "mig23", 1,
    FBAirRadarSpec{52000.0, 39000.0, 30.0, 0.0, 6.0, 2.0, /*LookDown*/ 14000.0, 40.0, false},
    /*HasRwr*/ true, 0.0, 0.0, false, true,
    /*Stations*/ 6, FBGunKind::Gsh23l, 200,
    FBAirPerf{320.0, 6.5, 8.5, 18.0, 200.0, 340.0, 155.0, 0.206, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Mig23, 13.965, 16.7)};

/* MiG-25PD: reach and no turn. Every other fighter here is a +7 to +9 g airframe; a limiter at +4.5 g
 * CANNOT TURN WITH ANYTHING, and that is a sourced property of the real aeroplane (aileron reversal,
 * 70 cm of wingtip flex) rather than a modelling weakness — the exact case the attribution test of
 * module.md §Spec 11 exists to distinguish. It reaches Mach 3.1, it sees 120 km, it shoots an 80 km
 * missile, and it loses every turning fight it enters. TrackRangeM is the LOWER bound of the sourced
 * 50-70 km, stated. */
inline constexpr FBAircraftSpec kMig25{
    "mig25", "MiG-25PD", FBAirTier::Gci, "mig25", 2,
    FBAirRadarSpec{120000.0, 50000.0, 60.0, 0.0, 10.5, 4.0, -1.0, 40.0, false},
    /*HasRwr*/ true, /*Irst*/ 25000.0, 50000.0, false, true,
    /*Stations*/ 4, FBGunKind::None, 0,
    FBAirPerf{400.0, 4.0, 4.5, 12.0, 250.0, 420.0, 180.0, 0.127, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Mig25, 14.01, 23.82)};

/* MiG-17F: guns and eyes, the catalogue's cheapest deck and its most honest row. SearchRangeM 0 —
 * acquisition is sensors/FBVisualSystem verbatim and therefore 3 784 m beam-on, 2 493 m head-on and
 * ZERO at night, measured and not set. It has no sensor to model, no missile envelope, no receiver and
 * no dispenser; everything about it that decides is in its eight anchors and its two guns. */
inline constexpr FBAircraftSpec kMig17{
    "mig17", "MiG-17F", FBAirTier::Visual, "mig17", 1,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ false, 0.0, 0.0, false, false,
    /*Stations*/ 2, FBGunKind::Nr23, 160,
    FBAirPerf{250.0, 5.0, 8.0, 20.0, 150.0, 280.0, 120.0, 0.271, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Mig17, 9.628, 11.264)};

/* Su-7BKL, the strike aircraft of O3. A5 IS [TODO], and that is a hard consequence rather than a
 * footnote: with no published g limit the alpha limiter is [SET] from the row's own measured CLmax and
 * the setting is logged; the row is ALPHA until it is measured. Its whole air-to-ground loadout costs
 * nothing — fab250/fab500 are already store rows with flown decks. */
inline constexpr FBAircraftSpec kSu7{
    "su7", "Su-7BKL", FBAirTier::Visual, "su7", 1,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ false, 0.0, 0.0, false, false,
    /*Stations*/ 6, FBGunKind::Nr30, 140,
    FBAirPerf{300.0, 5.0, 0.0, 18.0, 190.0, 320.0, 160.0, 0.198, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Su7, 9.31, 16.8)};

/* Su-17M4 / Su-22M4 — one row for the type O1 and O3 both name (the Su-20 is the export Su-17). The
 * anti-radiation stores it really carried are the ONE place in this catalogue where an eastern row
 * could do SEAD; named in the catalogue, not built. */
inline constexpr FBAircraftSpec kSu22{
    "su22", "Su-22M4", FBAirTier::Visual, "su22", 1,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ true, 0.0, 0.0, false, false,
    /*Stations*/ 10, FBGunKind::Nr30, 160,
    FBAirPerf{310.0, 5.5, 7.0, 18.0, 200.0, 330.0, 165.0, 0.185, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Su22, 13.68, 19.02)};

/* Mirage F1C: the ONLY pre-1980 row in the catalogue with a sourced look-down/shoot-down capability
 * (Cyrano IV, MTI and >40 dB clutter rejection), which is exactly why W2's Iraqi defence is a different
 * problem from O3's Egyptian one. SearchRangeM 96 km [SET within the disputed 96/100 band], the lower,
 * stated. A2 and A5 are both [TODO] and the row is ALPHA on the first of them. */
inline constexpr FBAircraftSpec kMirf1{
    "mirf1", "Mirage F1C", FBAirTier::Gci, "mirf1", 1,
    FBAirRadarSpec{96000.0, 40000.0, 60.0, 0.0, 10.5, 4.0, -1.0, 40.0, false},
    /*HasRwr*/ true, 0.0, 0.0, false, true,
    /*Stations*/ 5, FBGunKind::Defa553, 300,
    FBAirPerf{330.0, 6.0, 0.0, 20.0, 200.0, 340.0, 160.0, 0.209, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(Mirf1, 8.4, 15.3)};

/* F-5E Tiger II: NO CAMPAIGN NAMES THIS TYPE either (catalogue A11) — W1's aggressors are F-16s
 * emulating a Fulcrum by its own anchor. It is here because it carries a number nothing else does: the
 * published CD0 = 0.0200 and (L/D)max = 10.0 that DERIVE the whole catalogue's e = 0.66 and, as the
 * build found, its subsonic drag level too. Its APQ-159 range is [TODO], so until it is sourced this
 * row acquires through the eye like mig17 — the row WORKS without the number, and the number's absence
 * changes what the row is rather than breaking it. */
inline constexpr FBAircraftSpec kF5e{
    "f5e", "F-5E Tiger II", FBAirTier::Visual, "f5e", 2,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ false, 0.0, 0.0, false, false,
    /*Stations*/ 7, FBGunKind::M39a2, 560,
    FBAirPerf{300.0, 6.0, 0.0, 22.0, 180.0, 320.0, 145.0, 0.200, 0.0, 0.0},
    FBAirMoverSpec{}, 0.0, FB_AIR_LAYOUT(F5e, 8.13, 14.69)};

/* ============================ THE EIGHT MOVER ROWS ============================
 * No flight model, by the two-part test: their manoeuvre does not decide, and their drag polar is not
 * published — the data is absent BECAUSE the manoeuvre does not matter. What decides about each of them
 * is its speed, its altitude, its presented size and whether it radiates. */

/* E-3 Sentry, and the row that must not become a track source. It sees 400 km and it tells somebody,
 * which is precisely where a perception boundary falls by accident. What it publishes is an
 * FBNetReport: a POINT reconstructed from its own anonymous contact, carrying its own look age, with no
 * id field and no team field. A subscribing fighter re-centres its own antenna over the command bus and
 * must still FIND, FIRM and GATE the target itself.
 * SearchRangeM 400 km is the sourced LOW-FLYER figure, not the 650 km beyond-the-horizon one: the
 * campaigns' targets are fighters at low and medium altitude, and taking the larger number would claim
 * a mode against the wrong target class. FrameS 10.0 s [DERIVED from the sourced 6 rpm] — the ONE row
 * where the tree's volume/frame relation is CHECKED against a source, and the mechanism that keeps a
 * 400 km sensor from becoming a firing solution: a 10 s look age at a 250 m/s target is >= 2.5 km of
 * error the cued fighter has to search out itself.
 * EmitterKind stays AirborneFireControl — the tree has no early-warning value and adding one would
 * change what every RWR in the tree reports. The honest consequence, stated rather than fixed: a
 * fighter's receiver cannot tell an AWACS from a fighter radar. */
inline constexpr FBAircraftSpec kE3{
    "e3", "E-3 Sentry", FBAirTier::Reflex, "", 4,
    FBAirRadarSpec{400000.0, 0.0, 180.0, 0.0, 10.0, 10.0, -1.0, 40.0, false},
    /*HasRwr*/ true, 0.0, 0.0, /*NetNode*/ true, /*NetMember*/ false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{185.0, 237.0, 12500.0, 12.0, 25.0}, 0.0, FB_MOVER_LAYOUT(E3, 44.42, 46.61)};

/* E-2C Hawkeye: the same row with a carrier deck under it, and a separate row for one reason — it is
 * the AEW of O1, where the anchor states the E-2Cs "guided Israeli fighters to attack from the beam".
 * That is a GEOMETRY INSTRUCTION, which is exactly what this catalogue's cue is and exactly what it is
 * not allowed to become. Its ceiling of 10 600 m against the E-3's 8 800 m moves the radio horizon the
 * link is range-tested against. FrameS 10.0 [SET] at the E-3's sourced rate, for want of one of its
 * own. Its AN/ALR-73 ESM is NOT modelled as a second passive path: FlightBox has one passive receiver
 * and giving this row a different one would be a second perception path for one row. */
inline constexpr FBAircraftSpec kE2c{
    "e2c", "E-2C Hawkeye", FBAirTier::Reflex, "", 2,
    FBAirRadarSpec{640000.0, 0.0, 180.0, 0.0, 10.0, 10.0, -1.0, 40.0, false},
    /*HasRwr*/ true, 0.0, 0.0, /*NetNode*/ true, false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{132.0, 180.0, 10600.0, 12.0, 25.0}, 0.0, FB_MOVER_LAYOUT(E2c, 24.56, 17.6)};

/* KC-135R, and O1's Boeing 707 ECM aircraft for free — the same airframe family, and in the tree both
 * reduce to a large subsonic jet on a track with a published `jam_comm_m` ring and no weapon.
 * THE BOOM DOES NOT EXIST (C5) and naming this row does not move it one metre: the tanker is a
 * `protect` asset that cannot give fuel, so W2's defining constraint stays unexpressible. */
inline constexpr FBAircraftSpec kKc135{
    "kc135", "KC-135R Stratotanker", FBAirTier::Track, "", 4,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ false, 0.0, 0.0, false, false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{237.0, 259.0, 15240.0, 25.0, 25.0}, 0.0, FB_MOVER_LAYOUT(Kc135, 39.88, 41.53)};

/* Tu-95MS, the intercept subject. Its 50.1 m span is the row's decisive number: W5's and O2's task is
 * IDENTIFICATION and the eye resolves on presented extent, so a Tu-95 is recognised at roughly SEVEN
 * TIMES the range of a MiG-21 by the ratio of their extents alone. Nothing about that was set.
 * THE TAIL TURRET IS NOT MODELLED (module.md A6) — it is FBGunSystem with a rearward mount and one
 * hook, and it is the second thing to add, because a stern conversion against a bomber that shoots back
 * is a different problem from one against a bomber that does not. */
inline constexpr FBAircraftSpec kTu95{
    "tu95", "Tu-95MS", FBAirTier::Reflex, "", 4,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ true, 0.0, 0.0, false, false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{197.0, 257.0, 13716.0, 10.0, 25.0}, 0.0, FB_MOVER_LAYOUT(Tu95, 50.1, 46.2)};

/* An-26, the identification subject and the cheapest row in the catalogue. Its decisive quantities are
 * its span (29.3 m, for the eye) and its transponder state (`set iff_xpdr on|off`, an existing key) —
 * and that is the whole of W5's and O2's task. It also stands in for the Il-20 ELINT aircraft those two
 * campaigns name: an ELINT aircraft RECEIVES, it does not radiate, and FlightBox has no ESM emission to
 * model, so in the tree it is a transport on a track with different numbers. */
inline constexpr FBAircraftSpec kAn26{
    "an26", "An-26", FBAirTier::Track, "", 2,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ false, 0.0, 0.0, false, false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{122.0, 150.0, 7500.0, 8.0, 25.0}, 0.0, FB_MOVER_LAYOUT(An26, 29.3, 23.8)};

/* EF-111A Raven — a row of its own and not kc135 with a different name, because its FLIGHT PROFILE is
 * different in kind: supersonic, swing-wing, escorting a strike at strike speed rather than orbiting at
 * 300 kt, and speed and altitude are precisely the quantities a mover row carries. W3 names it six
 * times. ITS REAL JOB IS ABSENT: the ALQ-99 jammed RADARS, C13's radar half is wholly open, so this row
 * denies a LINK and nothing else. It is one third of the historical aircraft. */
inline constexpr FBAircraftSpec kEf111{
    "ef111", "EF-111A Raven", FBAirTier::Reflex, "", 2,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ true, 0.0, 0.0, false, false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{280.0, 653.0, 13716.0, 56.0, 25.0}, 0.0, FB_MOVER_LAYOUT(Ef111, 19.2, 23.16)};

/* Mi-8MT and AH-64: a rotorcraft is outside the recipe's fixed-wing set entirely, so a deck was never a
 * candidate. THE ONE THING THEY DO THAT DECIDES IS FREE: they are low and slow, so they sit inside the
 * Doppler notch of every radar in the tree. 240 km/h is 66.7 m/s, and any aspect that is not nearly
 * head-on puts the radial rate below every notch value the tree carries — a helicopter is therefore a
 * VISUAL and INFRARED target and not a radar one, without a line being written for it. */
inline constexpr FBAircraftSpec kMi8{
    "mi8", "Mi-8MT", FBAirTier::Reflex, "", 2,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ false, 0.0, 0.0, false, false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{66.7, 69.4, 5000.0, 8.0, 20.0}, 0.0, FB_MOVER_LAYOUT(Mi8, 21.29, 18.17)};

inline constexpr FBAircraftSpec kAh64{
    "ah64", "AH-64 Apache", FBAirTier::Reflex, "", 2,
    FBAirRadarSpec{0.0, 0.0, 0.0, 0.0, 0.0, 4.0, -1.0, 0.0, false},
    /*HasRwr*/ true, 0.0, 0.0, false, false,
    0, FBGunKind::None, 0, FBAirPerf{},
    FBAirMoverSpec{73.6, 101.3, 6100.0, 12.0, 20.0}, 0.0, FB_MOVER_LAYOUT(Ah64, 14.63, 17.73)};

#undef FB_AIR_ZONES
#undef FB_MOVER_ZONES
#undef FB_AIR_LAYOUT
#undef FB_MOVER_LAYOUT

} // namespace Aircraft

inline constexpr const FBAircraftSpec *kAircraftCatalogue[] = {
    &Aircraft::kE3, &Aircraft::kE2c, &Aircraft::kF15c, &Aircraft::kMig21, &Aircraft::kMig23,
    &Aircraft::kMig25, &Aircraft::kMig17, &Aircraft::kSu7, &Aircraft::kSu22, &Aircraft::kSu27,
    &Aircraft::kMirf1, &Aircraft::kF5e, &Aircraft::kKc135, &Aircraft::kTu95, &Aircraft::kAn26,
    &Aircraft::kEf111, &Aircraft::kMi8, &Aircraft::kAh64};

inline const FBAircraftSpec *FBFindAircraft(const char *key) {
  if (!key) return nullptr;
  for (const FBAircraftSpec *a : kAircraftCatalogue)
    if (std::strcmp(a->Key, key) == 0) return a;
  return nullptr;
}

} // namespace FlightBox
#endif
