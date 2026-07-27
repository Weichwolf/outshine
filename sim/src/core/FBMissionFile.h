/* FlightBox — FBMissionFile: the .fbm mission format parser (doc/mission-format.md). Pure
 * string-in/struct-out (no File I/O — the App reads the file and hands this the text, keeping core/
 * platform-neutral per the module architecture banner).
 *
 * A mission is MISSION-WIDE data (a name for the logs, an optional assigned FBRunway — landing geometry
 * + FBMissionMonitor's off-runway-touchdown check, shared by everyone flying this mission — and a
 * timeout in sim-seconds) plus a LIST of per-actor blocks (`unit <callsign>`), each carrying everything
 * that belongs to one flown entity: which module flies it, which faction it belongs to, its declarative
 * FBSpawn (position/altitude-or-ground/heading/speed — no modes), the `set <key> <value>` lines the
 * Runner hands that module verbatim during its spawn IC window (FBModule::ApplySetup interprets its own
 * keys), and the FBFlightPlan its `wp`/`land` lines build. One block = one FBSimUnit; N blocks = a
 * flight, which is why nothing here is a scalar "the module"/"the spawn" any more. */
#ifndef FBMISSIONFILE_H
#define FBMISSIONFILE_H

#include <string>
#include <utility>
#include <vector>
#include "FBFlightPlan.h"
#include "FBObjective.h"
#include "FBRunway.h"
#include "FBSpawn.h"
#include "FBTeam.h"

namespace FlightBox {

/* One actor block. `Id` is the callsign from the `unit` line — unique within a mission (the parser
 * rejects a duplicate), restricted to filename-safe characters because it also names this actor's own
 * telemetry file, and used verbatim as the unit attribution on every log line about this actor. */
struct FBMissionUnit {
  std::string  Id;
  std::string  ModuleName;   /* `module <name>` — resolved via FBModuleRegistry, e.g. "f16" */
  FBUnitTeam   Team = FBUnitTeam::Friendly;   /* `team` omitted = friendly */
  FBSpawn      Spawn;
  bool         HaveSpawn = false;
  FBFlightPlan Plan;         /* this actor's OWN waypoints; empty = an actor with nothing to fly to */
  /* `objective ...` lines, file order (core/FBObjective.h) — what this actor has to achieve against
   * the OTHER actors. Empty = the pre-combat behaviour: the flight plan is the whole judgement, and
   * being shot down is this unit's own failure and nobody's success. */
  std::vector<FBObjective> Objectives;
  std::vector<std::pair<std::string, std::string>> SetKV;   /* `set <key> <value>` lines, file order */
};

struct FBMission {
  std::string  Name;
  FBRunway     Runway;       /* optional: landing-relevant geometry, `spawn threshold ...`'s reference */
  bool         HaveRunway = false;
  double       TimeoutS = 0.0;   /* sim-seconds until TIMEOUT; 0 = unset (a parse error, not a mission) */
  std::vector<FBMissionUnit> Units;
};

/* Parses one .fbm mission (doc/mission-format.md): keyword lines, '#' end-of-line comments, blank lines
 * ignored, mission-wide keywords before the first `unit` block, everything actor-scoped inside one.
 * Returns false with a "line N: ..." message in *err (if given) on any malformed line, an actor keyword
 * outside a block, a mission keyword after one, a duplicate/unusable unit id, a `threshold`/`land`
 * without a runway, or a missing mandatory field — `out` is only fully valid on true. */
bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err = nullptr);

} // namespace FlightBox
#endif
