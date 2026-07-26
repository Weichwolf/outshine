/* FlightBox — FBMissionFile: the .fbm mission format parser (doc/mission-format.md). Pure
 * string-in/struct-out (no File I/O — the App reads the file and hands this the text, keeping core/
 * platform-neutral per the module architecture banner). One flat FBMission ties together what the
 * mission orchestrator (FBMissionRunner.h) needs: a name for the logs, the unit's declarative FBSpawn
 * (position/altitude-or-ground/heading/speed — no modes), an optional assigned FBRunway (landing-
 * relevant geometry + FBMissionMonitor's off-runway-touchdown FAIL check; NOT required — an airborne-
 * only mission has none), the FBFlightPlan `wp`/`land` lines build, a flat `set <key> <value>` list the
 * Runner hands the module verbatim during the spawn IC window (FBModule::ApplySetup interprets its own
 * keys), and a timeout in sim-seconds. */
#ifndef FBMISSIONFILE_H
#define FBMISSIONFILE_H

#include <string>
#include <utility>
#include <vector>
#include "FBFlightPlan.h"
#include "FBRunway.h"
#include "FBSpawn.h"

namespace FlightBox {

/* Today's FBMission describes ONE controllable unit (module + spawn + flight plan) — a real
 * constraint of today's mission runner (FBMissionRunner.h), not a data-model dead end: a future
 * multi-unit mission (a flight of several modules/factions, e.g. an F-16 vs. a MiG-29) is a Mission
 * holding a LIST of these per-unit blocks, not a redesign of this one. Nothing here assumes "exactly
 * one module" beyond what this round's runner itself does. */
struct FBMission {
  std::string  Name;
  std::string  ModuleName;   /* `module <name>` — resolved via FBModuleRegistry, e.g. "f16" */
  FBSpawn      Spawn;
  bool         HaveSpawn = false;
  FBRunway     Runway;       /* optional: landing-relevant geometry, `spawn threshold ...`'s reference */
  bool         HaveRunway = false;
  FBFlightPlan Plan;
  std::vector<std::pair<std::string, std::string>> SetKV;   /* `set <key> <value>` lines, file order */
  double       TimeoutS = 0.0;   /* sim-seconds until TIMEOUT; 0 = unset (a parse error, not a valid mission) */
};

/* Parses one .fbm mission (doc/mission-format.md): keyword lines, '#' end-of-line comments, blank
 * lines ignored. Returns false with a "line N: ..." message in *err (if given) on any malformed line,
 * a takeoff/land keyword before runway, or a missing name/runway/timeout — `out` is only fully valid
 * on true. */
bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err = nullptr);

} // namespace FlightBox
#endif
