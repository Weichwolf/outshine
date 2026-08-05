/* WHAT A FLOWN AIRFRAME HAS — the schema, and deliberately not one instance of it. WHICH airframes
 * exist is a scenario's statement, declared in a mod's catalogue manifest and loaded through
 * missions/FBCatalogueBoot.h into core/FBAircraftCatalogue.h; core/ describes the shape and never the
 * cast (CLAUDE.md Prinzip 3). A field's meaning is here, a row's numbers and their sources are the
 * manifest's own and doc/modules/air/catalogue.md's.
 *
 * THE THIRD LEVEL (doc/modules/air/module.md §Spec 1). A MODULE is a flown airframe FlightBox is judged
 * on — a pinned or anchor-measured deck, ~20 classes and a reference base directory of its own. A
 * CATALOGUE CELL is a flown airframe FlightBox is not judged on — one parametric class, one manifest
 * row, a GENERATED deck or no deck at all. A UNIT is everything perceived and shot at without being
 * flown.
 *
 * TWO MOTION LAWS AND NO ROW GETS BOTH: a row flies on JSBSim iff its own manoeuvre decides an outcome
 * AND its envelope is published. Those two are the same fact seen twice — fighter data IS envelope
 * data — so the test never splits a row. */
#ifndef FBAIRCRAFT_H
#define FBAIRCRAFT_H

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
  Peer,           /* T4: the whole phase machine, formation included — every `set task` the module
                   * layer defines, with nothing withheld */
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
   * period for any airborne fire-control radar. A row whose rotation rate IS sourced says so and is
   * where the relation gets CHECKED. */
  double FrameS = 4.0;
  /* THE LOOK-DOWN GATE, and on the sets that lack it, it is the whole aircraft. 0 = the set cannot look
   * down at all; positive = the reduced range it manages against a target BELOW the antenna's own
   * horizon. Negative sentinel -1 = no limit. */
  double LookDownRangeM = -1.0;
  double DopplerNotchMs = 0.0;    /* 0 = no clutter gate: a pulse set cannot be chaffed at all */
  bool   NotchRejects = false;
};

/* THE AIRFRAME NUMBERS pilot/FBPilot reads as virtual hooks. The last two are the TIER GATE: a row
 * whose roll plant is unmeasured is T0 or T1 and the boot REFUSES `set task bfm` with SET_REJECTED,
 * because doc/pilot.md's close-combat law INVERTS the plant and with another airframe's it is not a
 * limiter but an oscillator (measured across the flown airframes: 2.6x spread in roll response for the
 * same stick, doc/modules/air/module.md). */
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
   * 1.5*|Cma|*a_lim/(|Cmde|*de_max) on its own generated deck. It used to be one [SET] number on every
   * row and nothing read it — every row flew on one preset, which cost air-bomber-intercept.fbm
   * 8 000 m of altitude in 242 s. */
  double PitchStickMax = 0.0;
  /* 0 = UNMEASURED, and the tier gate then refuses `set task bfm` on this row. A number here was
   * MEASURED by `make -C sim test-air` (anchors ROLL-A / ROLL-K) on this row's own generated deck and
   * written back by recipe STEP 8, which grants a tier only once the deck has also passed the deviation
   * bands of §7.1 — so an ALPHA row keeps a zero even though the harness can print its plant. */
  double RollPlantA = 0.0;
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
  /* AN EARLY-WARNING NODE, and this is the schema's most dangerous bool. True = the row publishes an
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

  /* 0 = NOT DECLARED, and a manifest is expected to leave it so (catalogue A3): the tree holds exactly
   * two cross-sections measured against each other, and the sigma^(1/4) law makes a wrong value cheap
   * to spot — which is exactly why inventing the rest is worse than declaring none. The radar then
   * reads "no scaling", i.e. its own calibration range (sensors/FBRadarSystem.h kRefRcsM2). */
  double RcsM2 = 0.0;

  /* DERIVED from the row's declared span and length by FBAircraftCatalogue, never declared: a layout is
   * a consequence of the two dimensions every published source carries, not a table of its own. */
  FBDamageLayout Layout{};

  bool IsMover() const { return FdmModel[0] == '\0'; }
  /* WHETHER THIS ROW GETS A FIRE CONTROL AT ALL, and the rule is the two facts the row already states:
   * a tier that has a combat phase, and a weapon declared. A T0 tanker and a T1 early-warning node
   * therefore never write FBState::FireControl, which is not an omission but the tier — the pilot
   * staffelung of doc/modules/air/module.md §Spec 5 says T0 reacts to nothing and T1 gets exactly one
   * reflex. */
  bool CanEmploy() const {
    return Tier >= FBAirTier::Visual && (Stations > 0 || Gun != FBGunKind::None);
  }
};

} // namespace FlightBox
#endif
