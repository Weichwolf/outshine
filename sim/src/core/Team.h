/* The faction an entity belongs to. In core/ because it is BOTH world-entity identity and mission
 * DATA — duplicating it would give the mission file and the world two notions of "hostile". */
#ifndef TEAM_H
#define TEAM_H

#include <cstring>

namespace outshine {

enum class UnitTeam { Friendly, Hostile, Neutral };

inline const char *UnitTeamStr(UnitTeam t) {
  switch (t) {
    case UnitTeam::Friendly: return "friendly";
    case UnitTeam::Hostile:  return "hostile";
    case UnitTeam::Neutral:  return "neutral";
  }
  return "?";
}

/* The `team` keyword's only accepted spellings — the parser is strict everywhere else too. */
inline bool UnitTeamFromString(const char *s, UnitTeam &out) {
  if (!std::strcmp(s, "friendly")) { out = UnitTeam::Friendly; return true; }
  if (!std::strcmp(s, "hostile"))  { out = UnitTeam::Hostile;  return true; }
  if (!std::strcmp(s, "neutral"))  { out = UnitTeam::Neutral;  return true; }
  return false;
}

} // namespace outshine
#endif
