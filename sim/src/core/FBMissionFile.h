/* The .fbm mission format parser (doc/missions/syntax.md). Pure string-in/struct-out — the App reads the
 * file, so core/ stays platform-neutral. A mission is MISSION-WIDE data plus a LIST of per-actor
 * blocks: one block = one FBSimUnit. doc/core.md, Abschnitt 5.1. */
#ifndef FBMISSIONFILE_H
#define FBMISSIONFILE_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "FBFlight.h"
#include "FBFlightPlan.h"
#include "FBNetReport.h"
#include "FBObjective.h"
#include "FBRunway.h"
#include "FBSpawn.h"
#include "FBTeam.h"
#include "FBZone.h"

namespace FlightBox {

/* `Id` is the `unit` line's callsign: unique per mission, filename-safe (it names this actor's own
 * telemetry file) and used verbatim as the log attribution. */
struct FBMissionUnit {
  std::string  Id;
  std::string  ModuleName;   /* `module <name>` — resolved via FBModuleRegistry, e.g. "f16" */
  FBUnitTeam   Team = FBUnitTeam::Friendly;   /* `team` omitted = friendly */
  /* `flight <name> <position>` — omitted leaves Position 0, and every piece of flight behaviour is
   * then a no-op: a mission written before flights existed flies byte-identically. */
  FBFlightId   Flight;
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

/* One member of a `net` block: which position, the arc it is responsible for, and what it does when the
 * node goes quiet. Nothing here is a performance figure — capability stays catalogue data. */
struct FBNetMember {
  std::string      Id;
  bool             HaveSector = false;   /* false = all-round; a cue is then never out of sector */
  double           SectorCentreDeg = 0.0, SectorHalfDeg = 180.0;
  FBWeaponsControl Autonomy = FBWeaponsControl::Hold;   /* the fallback doctrine, [SET] per mission */
};

/* `net <name>` — the DOCTRINE of one connected air defence, and deliberately not `set` keys on the
 * positions: a battery does not know it is in a net until somebody puts it in one, and the C1 contract
 * declares exactly six author-facing site keys. doc/air-defence-network.md §8. */
struct FBNetSpec {
  std::string  Name;
  std::string  Control;                 /* the node's callsign; empty = no node, every member autonomous */
  bool         Wire = false;            /* `link wire` — no horizon test, and unjammable */
  double       RangeM = 0.0;            /* `link radio <rangeM>`; 0 with radio = the terminal's own reach */
  double       MastM = 0.0;             /* [SET] antenna height above ground, both ends of the link */
  double       PeriodS = 1.0;           /* [SET] the net's reporting cycle */
  double       HoldCycles = 3.0;        /* [SET] cycles of silence before a member goes Silent */
  FBWeaponsControl Wcs = FBWeaponsControl::Free;   /* what the NODE transmits */
  std::vector<FBNetMember> Members;
};

struct FBMission {
  std::string  Name;
  FBRunway     Runway;       /* optional: landing-relevant geometry, `spawn threshold ...`'s reference */
  bool         HaveRunway = false;
  double       TimeoutS = 0.0;   /* sim-seconds until TIMEOUT; 0 = unset (a parse error, not a mission) */
  FBWeatherSpec Weather;
  bool         HaveWeather = false;   /* declared explicitly — the ONE thing that outranks a client default */
  /* `time <YYYY-MM-DDThh:mm:ssZ>` — the UTC instant at simT = 0. ABSENT MEANS NO CLOCK, not a default
   * epoch: nothing is then computed, published or logged, which is what keeps every mission written
   * before the clock existed byte-identical. doc/missions/syntax.md, "The mission clock". */
  int64_t      UtcT0S = 0;
  bool         HaveTime = false;
  /* Declared geometry the JUDGE reads and no unit ever does, and the doctrine of the nets over the
   * cast. Both are mission-wide and both are empty for every file written before they existed. */
  std::vector<FBZone>    Zones;
  std::vector<FBNetSpec> Nets;
  std::vector<FBMissionUnit> Units;
};

/* Mission-wide keywords before the first `unit` block, everything actor-scoped inside one. Returns
 * false with a "line N: ..." message in *err; `out` is only fully valid on true. */
bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err = nullptr);

} // namespace FlightBox
#endif
