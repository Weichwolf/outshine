/* The faction an entity belongs to. In core/ because it is BOTH world-entity identity and mission
 * DATA — duplicating it would give the mission file and the world two notions of "hostile". */
#ifndef FBTEAM_H
#define FBTEAM_H

#include <cstring>

namespace FlightBox {

enum class FBUnitTeam { Friendly, Hostile, Neutral };

inline const char *FBUnitTeamStr(FBUnitTeam t) {
  switch (t) {
    case FBUnitTeam::Friendly: return "friendly";
    case FBUnitTeam::Hostile:  return "hostile";
    case FBUnitTeam::Neutral:  return "neutral";
  }
  return "?";
}

/* The `team` keyword's only accepted spellings — the parser is strict everywhere else too. */
inline bool FBUnitTeamFromString(const char *s, FBUnitTeam &out) {
  if (!std::strcmp(s, "friendly")) { out = FBUnitTeam::Friendly; return true; }
  if (!std::strcmp(s, "hostile"))  { out = FBUnitTeam::Hostile;  return true; }
  if (!std::strcmp(s, "neutral"))  { out = FBUnitTeam::Neutral;  return true; }
  return false;
}

} // namespace FlightBox
#endif
