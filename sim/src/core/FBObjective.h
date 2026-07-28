/* A COMBAT objective as mission data: what a unit has to achieve against the OTHER units. It is what
 * makes one observed fact readable from two sides — the loser's FAIL and the shooter's SUCCESS are the
 * same shot. It stays an OBSERVATION: evaluated against the roster the client fills from the health
 * registers it owns. doc/core.md, Abschnitt 5.5. */
#ifndef FBOBJECTIVE_H
#define FBOBJECTIVE_H

#include <cstring>
#include <string>
#include <vector>
#include "FBTeam.h"

namespace FlightBox {

/* Append only: the ordinal is not telemetry-visible, but the names are the .fbm spelling. */
enum class FBObjectiveKind {
  Survive,    /* stay combat-effective to the end of the run */
  KillUnit,   /* one named unit is to be made combat-ineffective */
  KillTeam,   /* every unit of one faction is */
  /* Implicit for a unit that declares NO objectives; declared explicitly it is how a unit that DOES
   * declare objectives keeps it — an `objective` block is the COMPLETE statement. */
  Waypoints
};

struct FBObjective {
  FBObjectiveKind Kind = FBObjectiveKind::Survive;
  std::string     TargetId;                            /* KillUnit: the target's callsign */
  FBUnitTeam      TargetTeam = FBUnitTeam::Hostile;    /* KillTeam: the target faction */
};

/* Deliberately nothing beyond who it is, whose side it is on and whether it can still fight — no
 * position, no self-report, no module handle. `Id` borrows the unit's name for the tick. */
struct FBUnitObservation {
  const char *Id = "";
  FBUnitTeam  Team = FBUnitTeam::Friendly;
  bool        CombatEffective = true;
};

/* A borrowed VIEW, not a container: it is handed to every judged unit every tick, and the tick path
 * allocates nothing. */
struct FBMissionRoster {
  const FBUnitObservation *Units = nullptr;
  int Count = 0;
};

/* The one shared primitive behind both questions asked of an objective: the monitor's "is it met yet"
 * and the runner's "was this loss somebody's declared goal". */
inline bool FBObjectiveCovers(const FBObjective &o, const char *id, FBUnitTeam team) {
  switch (o.Kind) {
    case FBObjectiveKind::KillUnit: return id && o.TargetId == id;
    case FBObjectiveKind::KillTeam: return o.TargetTeam == team;
    case FBObjectiveKind::Survive:
    case FBObjectiveKind::Waypoints: return false;
  }
  return false;
}

/* Every named unit combat-ineffective AND at least one named: an objective against a faction nobody
 * belongs to is never met rather than vacuously true — a misspelt enemy should not pass. */
inline bool FBObjectiveMet(const FBObjective &o, const FBMissionRoster &roster) {
  if (o.Kind == FBObjectiveKind::Survive || o.Kind == FBObjectiveKind::Waypoints) return false;
  bool any = false;
  for (int i = 0; i < roster.Count; i++) {
    if (!FBObjectiveCovers(o, roster.Units[i].Id, roster.Units[i].Team)) continue;
    any = true;
    if (roster.Units[i].CombatEffective) return false;
  }
  return any;
}

/* The .fbm spelling, for logs and for the parser's error messages. */
inline std::string FBObjectiveStr(const FBObjective &o) {
  switch (o.Kind) {
    case FBObjectiveKind::Survive:   return "survive";
    case FBObjectiveKind::KillUnit:  return "kill unit " + o.TargetId;
    case FBObjectiveKind::KillTeam:  return std::string("kill team ") + FBUnitTeamStr(o.TargetTeam);
    case FBObjectiveKind::Waypoints: return "waypoints";
  }
  return "?";
}

} // namespace FlightBox
#endif
