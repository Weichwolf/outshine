/* The .fbm mission format parser (doc/mission-format.md). Pure string-in/struct-out — the App reads the
 * file, so core/ stays platform-neutral. A mission is MISSION-WIDE data plus a LIST of per-actor
 * blocks: one block = one FBSimUnit. doc/flightbox/core.md, Abschnitt 5.1. */
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

/* `Id` is the `unit` line's callsign: unique per mission, filename-safe (it names this actor's own
 * telemetry file) and used verbatim as the log attribution. */
struct FBMissionUnit {
  std::string  Id;
  std::string  ModuleName;   /* `module <name>` — resolved via FBModuleRegistry, e.g. "f16" */
  FBUnitTeam   Team = FBUnitTeam::Friendly;   /* `team` omitted = friendly */
  FBSpawn      Spawn;
  bool         HaveSpawn = false;
  FBFlightPlan Plan;         /* this actor's OWN waypoints; empty = an actor with nothing to fly to */
  /* Empty = the flight plan is the whole judgement, and being shot down is this unit's own failure
   * and nobody's success. */
  std::vector<FBObjective> Objectives;
  std::vector<std::pair<std::string, std::string>> SetKV;   /* `set <key> <value>` lines, file order */
};

/* `wx <kind> ...` — mission-wide, because an atmosphere is not something one actor has and another does
 * not. Unset means Calm, so a file written before weather existed declares still air by omission. */
enum class FBWeatherKind { Calm, Fixture, Wind };

struct FBWeatherSpec {
  FBWeatherKind Kind = FBWeatherKind::Calm;
  std::string   Fixture;       /* `wx fixture <name|path>`: a bare name resolves under the client's assets */
  double        WindFromDeg = 0.0, WindSpeedKt = 0.0;   /* `wx wind <dirFROM> <kt>` */
};

struct FBMission {
  std::string  Name;
  FBRunway     Runway;       /* optional: landing-relevant geometry, `spawn threshold ...`'s reference */
  bool         HaveRunway = false;
  double       TimeoutS = 0.0;   /* sim-seconds until TIMEOUT; 0 = unset (a parse error, not a mission) */
  FBWeatherSpec Weather;
  bool         HaveWeather = false;   /* declared explicitly — the ONE thing that outranks a client default */
  std::vector<FBMissionUnit> Units;
};

/* Mission-wide keywords before the first `unit` block, everything actor-scoped inside one. Returns
 * false with a "line N: ..." message in *err; `out` is only fully valid on true. */
bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err = nullptr);

} // namespace FlightBox
#endif
