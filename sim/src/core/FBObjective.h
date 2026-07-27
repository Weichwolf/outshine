/* FlightBox — FBObjective: a COMBAT objective as mission data. The .fbm format's `wp`/`land` lines
 * already declare what a unit has to REACH; this declares what it has to achieve against the other
 * units in the mission — stay combat-effective, or make somebody else stop being it.
 *
 * WHY THE FORMAT NEEDED IT (doc/mission-format.md, "Urteil"): before this, the only thing a weapon hit
 * could produce was the FAILURE of whoever was hit, because no unit could declare that this failure was
 * its own goal. A mission whose hostile unit was shot down therefore ended as FAIL — the verdict was
 * team-blind. An objective is what makes the same observed fact readable from two sides at once: the
 * loser's FAIL and the shooter's SUCCESS are the same shot.
 *
 * IT STAYS AN OBSERVATION, NOT A CLAIM. An objective is evaluated by core/FBMissionMonitor against the
 * roster below — id, faction and the one bit core/FBSystemHealth publishes about a unit — which the
 * client fills from the health registers it owns, exactly like it fills the monitor's position sample.
 * A module can no more declare its opponent dead than it can declare itself landed. */
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
  /* Reach every `wp`/`land` line of this unit's own flight plan. It is what a unit that declares NO
   * objectives is judged on implicitly (the format's original and only judgement), and declaring it
   * explicitly is how a unit that DOES declare objectives keeps it: an `objective` block is the
   * COMPLETE statement of what that unit has to achieve, so a combat mission's briefed vector stops
   * being something it has to fly to unless the file says it is. */
  Waypoints
};

struct FBObjective {
  FBObjectiveKind Kind = FBObjectiveKind::Survive;
  std::string     TargetId;                            /* KillUnit: the target's callsign */
  FBUnitTeam      TargetTeam = FBUnitTeam::Hostile;    /* KillTeam: the target faction */
};

/* ONE observed unit of the mission's cast, as the judge is shown it: who it is, whose side it is on,
 * and whether it can still fight (core/FBSystemHealth::CombatEffective). Deliberately nothing else —
 * no position, no self-report, no module handle. `Id` borrows the unit's own name for the tick. */
struct FBUnitObservation {
  const char *Id = "";
  FBUnitTeam  Team = FBUnitTeam::Friendly;
  bool        CombatEffective = true;
};

/* A borrowed view over the caller's per-tick roster buffer (one entry per non-weapon actor). A view
 * rather than a container because it is handed to every judged unit's monitor on every tick: the
 * client fills one reused vector, nothing allocates in the tick path. */
struct FBMissionRoster {
  const FBUnitObservation *Units = nullptr;
  int Count = 0;
};

/* Does this objective NAME that unit — i.e. is that unit one of the units it is about? The one shared
 * primitive behind both questions asked of an objective: the monitor's "is it met yet" (below) and the
 * runner's "was this loss somebody's declared goal" (app/FBMissionRunner.cpp's combination rule). */
inline bool FBObjectiveCovers(const FBObjective &o, const char *id, FBUnitTeam team) {
  switch (o.Kind) {
    case FBObjectiveKind::KillUnit: return id && o.TargetId == id;
    case FBObjectiveKind::KillTeam: return o.TargetTeam == team;
    case FBObjectiveKind::Survive:
    case FBObjectiveKind::Waypoints: return false;
  }
  return false;
}

/* Is a KILL objective met? Every unit it names has to be combat-ineffective, and it has to name at
 * least one — an objective against a faction nobody in this mission belongs to is never met rather
 * than vacuously true, because a mission that misspells its enemy should not pass. A Survive objective
 * is NOT decided here: it is only met at the end of the run (see FBMissionMonitor). */
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
