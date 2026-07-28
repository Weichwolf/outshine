/* A COMBAT objective as mission data: what a unit has to achieve against the OTHER units. It is what
 * makes one observed fact readable from two sides — the loser's FAIL and the shooter's SUCCESS are the
 * same shot. It stays an OBSERVATION: evaluated against the roster the client fills from the health
 * registers it owns. doc/core.md, Abschnitt 5.5. */
#ifndef FBOBJECTIVE_H
#define FBOBJECTIVE_H

#include <cstring>
#include <limits>
#include <sstream>
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
  Waypoints,
  /* The four of doc/missions/verdict.md, "The four new objective kinds". Every one of them is checked
   * against what the aircraft DID, never against what it knew: a geometry, a health bit, a release bit. */
  Identify,     /* hold a planar range to a named unit for a cumulative dwell — the flown pass */
  Protect,      /* the named unit(s) are still combat-effective at the END of the run */
  NoFire,       /* this unit released nothing and fired no burst for the whole run */
  DenyRelease,  /* the named unit(s) released nothing for the whole run */
  /* Cumulative dwell inside a DECLARED cylinder stays at or below an exposure budget. Admissible under
   * doc/missions/verdict.md's own test — it measures what the aircraft DID, in the identical currency
   * `identify`'s planar range already uses — and it costs the roster nothing, because it asks about the
   * declaring unit's own sample. doc/air-defence-network.md §4. */
  AvoidZone,
  /* The named unit's CUMULATIVE RADIATING TIME over the run stays at or below an allowance. A BEHAVIOUR
   * and not a damage state: it is scored against an OBSERVED bit on a published signature, the same
   * class of fact as a health bit, and never against a new FBSystemId (the register is monotone by
   * contract and suppression is not) nor by widening CombatEffective() (which is a statement about
   * airframes and is read for every unit in the tree). Destroyed implies suppressed and that is not a
   * special case: Radar failed -> the block goes Invalid -> Emission() returns None -> the bit is false
   * forever. doc/air-to-ground.md §5.2. */
  Suppress
};

/* PER-OBJECTIVE, not per unit: what the judge decided about ONE declared goal. `Violated` is reserved
 * for the three kinds that have a violation condition of their own (doc/missions/verdict.md's
 * "Violated when" column) — everything else is simply unmet. It exists because an all-or-nothing bool
 * cannot say WHICH of several goals a doctrine reached, which is the middle level of the fitness in
 * doc/doctrine-evolution.md §1.3. */
enum class FBObjectiveState { Unmet, Met, Violated };

inline const char *FBObjectiveStateStr(FBObjectiveState s) {
  switch (s) {
    case FBObjectiveState::Unmet: return "unmet";
    case FBObjectiveState::Met: return "met";
    case FBObjectiveState::Violated: return "violated";
  }
  return "?";
}

/* Which side of `unit <callsign>` / `team <faction>` a kind that offers both was written on. KillUnit/
 * KillTeam predate it and carry the discriminator in the KIND; the new pair carries it here so that
 * `protect`/`deny release` stay ONE kind each, as doc/core.md §5.5 declares them. */
enum class FBObjectiveScope { Unit, Team };

struct FBObjective {
  FBObjectiveKind  Kind = FBObjectiveKind::Survive;
  FBObjectiveScope Scope = FBObjectiveScope::Unit;      /* Protect/DenyRelease: which target set */
  std::string      TargetId;                            /* KillUnit/Identify + Scope::Unit: the callsign */
  FBUnitTeam       TargetTeam = FBUnitTeam::Hostile;    /* KillTeam + Scope::Team: the faction */
  double           RangeM = 0.0;                        /* Identify: the box, [SET] per mission */
  double           HoldS = 0.0;                         /* Identify: the cumulative dwell inside it;
                                                         * AvoidZone: the exposure budget;
                                                         * Suppress: the radiating allowance */
};

/* Deliberately nothing beyond who it is, whose side it is on, whether it can still fight, whether it
 * has ever let anything go, and how far away it is — no position, no self-report, no module handle.
 * `Id` borrows the unit's name for the tick. Both of the last two are filled by the OWNER from
 * registers the owner holds itself: the release bit from the queue it drains at the growth phase, the
 * range from the published poses the CPA already runs on. Their defaults are "nothing happened", so a
 * mission that declares none of the new kinds never reads either. */
struct FBUnitObservation {
  const char *Id = "";
  FBUnitTeam  Team = FBUnitTeam::Friendly;
  bool        CombatEffective = true;
  bool        ReleasedWeapon = false;   /* monotone over the run: a release or a burst, ever */
  /* IS THIS UNIT RADIATING RIGHT NOW — filled by the OWNER from the signature the unit publishes at the
   * barrier, the identical construction the two bits above use. It is the FIRST non-monotone roster
   * field, and that is safe because the judge's ACCUMULATOR is monotone, which is what the rule needs.
   * doc/air-to-ground.md §5.2. */
  bool        Emitting = false;
  double      RangeM = std::numeric_limits<double>::infinity();   /* planar, FROM the judged unit */
};

/* A borrowed VIEW, not a container: it is handed to every judged unit every tick, and the tick path
 * allocates nothing. */
struct FBMissionRoster {
  const FBUnitObservation *Units = nullptr;
  int Count = 0;
};

/* Does this objective's TARGET SET contain that unit? Pure addressing, no verdict — which is what lets
 * `protect` and `kill` share it while meaning opposite things. */
inline bool FBObjectiveNames(const FBObjective &o, const char *id, FBUnitTeam team) {
  switch (o.Kind) {
    case FBObjectiveKind::KillUnit:
    case FBObjectiveKind::Identify: return id && o.TargetId == id;
    case FBObjectiveKind::KillTeam: return o.TargetTeam == team;
    case FBObjectiveKind::Protect:
    case FBObjectiveKind::DenyRelease:
    case FBObjectiveKind::Suppress:
      return o.Scope == FBObjectiveScope::Unit ? (id && o.TargetId == id) : o.TargetTeam == team;
    case FBObjectiveKind::Survive:
    case FBObjectiveKind::Waypoints:
    case FBObjectiveKind::NoFire:
    case FBObjectiveKind::AvoidZone: return false;   /* they are about the declaring unit itself */
  }
  return false;
}

/* "Somebody declared this unit's loss as their goal" — the runner's combination rule reads it to decide
 * whether a loss was EXPECTED and therefore stops deciding the run. ONLY a kill does that. `protect`
 * names a unit for the opposite reason, and letting it cover would make the protected unit's death the
 * expected outcome of the declaration that it must not happen (doc/missions/verdict.md, C12). */
inline bool FBObjectiveCovers(const FBObjective &o, const char *id, FBUnitTeam team) {
  switch (o.Kind) {
    case FBObjectiveKind::KillUnit:
    case FBObjectiveKind::KillTeam: return FBObjectiveNames(o, id, team);
    case FBObjectiveKind::Survive:
    case FBObjectiveKind::Waypoints:
    case FBObjectiveKind::Identify:
    case FBObjectiveKind::Protect:
    case FBObjectiveKind::NoFire:
    case FBObjectiveKind::DenyRelease:
    case FBObjectiveKind::AvoidZone:
    /* Wanting a position QUIET is not declaring that it should die — the same argument `protect` makes
     * for the opposite reason, and the reason a suppression sortie's target still decides the run if it
     * is lost to something nobody declared. */
    case FBObjectiveKind::Suppress: return false;
  }
  return false;
}

/* The roster-decidable kinds, all three of the same shape: at least one named unit, and every named one
 * satisfies the kind's predicate. An objective against a faction nobody in this mission belongs to is
 * never met rather than vacuously true — a misspelt enemy should not pass. `Survive`/`Waypoints`/
 * `NoFire` are about the declaring unit and `Identify` needs a dwell the roster cannot carry; all four
 * are answered by FBMissionMonitor. */
inline bool FBObjectiveMet(const FBObjective &o, const FBMissionRoster &roster) {
  switch (o.Kind) {
    case FBObjectiveKind::KillUnit:
    case FBObjectiveKind::KillTeam:
    case FBObjectiveKind::Protect:
    case FBObjectiveKind::DenyRelease: break;
    case FBObjectiveKind::Survive:
    case FBObjectiveKind::Waypoints:
    case FBObjectiveKind::Identify:
    case FBObjectiveKind::NoFire:
    case FBObjectiveKind::AvoidZone:
    case FBObjectiveKind::Suppress: return false;   /* needs a dwell the roster cannot carry */
  }
  bool any = false;
  for (int i = 0; i < roster.Count; i++) {
    const FBUnitObservation &u = roster.Units[i];
    if (!FBObjectiveNames(o, u.Id, u.Team)) continue;
    any = true;
    bool ok = o.Kind == FBObjectiveKind::Protect       ? u.CombatEffective
            : o.Kind == FBObjectiveKind::DenyRelease   ? !u.ReleasedWeapon
                                                       : !u.CombatEffective;
    if (!ok) return false;
  }
  return any;
}

/* The .fbm spelling, for logs and for the parser's error messages. */
inline std::string FBObjectiveStr(const FBObjective &o) {
  auto target = [&o]() {
    return o.Scope == FBObjectiveScope::Unit ? "unit " + o.TargetId
                                             : std::string("team ") + FBUnitTeamStr(o.TargetTeam);
  };
  switch (o.Kind) {
    case FBObjectiveKind::Survive:   return "survive";
    case FBObjectiveKind::KillUnit:  return "kill unit " + o.TargetId;
    case FBObjectiveKind::KillTeam:  return std::string("kill team ") + FBUnitTeamStr(o.TargetTeam);
    case FBObjectiveKind::Waypoints: return "waypoints";
    case FBObjectiveKind::Identify: {
      std::ostringstream os;
      os << "identify unit " << o.TargetId << " range " << o.RangeM << " hold " << o.HoldS;
      return os.str();
    }
    case FBObjectiveKind::Protect:     return "protect " + target();
    case FBObjectiveKind::NoFire:      return "no_fire";
    case FBObjectiveKind::DenyRelease: return "deny release " + target();
    case FBObjectiveKind::AvoidZone: {
      std::ostringstream os;
      os << "avoid zone " << o.TargetId << " exposure " << o.HoldS;
      return os.str();
    }
    case FBObjectiveKind::Suppress: {
      std::ostringstream os;
      os << "suppress " << target() << " emitting " << o.HoldS;
      return os.str();
    }
  }
  return "?";
}

} // namespace FlightBox
#endif
