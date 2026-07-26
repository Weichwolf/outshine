/* FlightBox — FBUnitTeam: the faction an entity belongs to. It lives in core/ rather than beside
 * FBUnit because it is BOTH world-entity identity (units/FBUnit) and mission DATA (a .fbm `team` line,
 * core/FBMissionFile) — parking it in units/ would make core/ depend on units/ just to name a faction,
 * and duplicating the enum would give the mission file and the world two notions of "hostile". */
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

/* The .fbm `team` keyword's only accepted spellings (lowercase, exactly the FBUnitTeamStr strings) —
 * the parser is strict everywhere else, so it is strict here too. */
inline bool FBUnitTeamFromString(const char *s, FBUnitTeam &out) {
  if (!std::strcmp(s, "friendly")) { out = FBUnitTeam::Friendly; return true; }
  if (!std::strcmp(s, "hostile"))  { out = FBUnitTeam::Hostile;  return true; }
  if (!std::strcmp(s, "neutral"))  { out = FBUnitTeam::Neutral;  return true; }
  return false;
}

} // namespace FlightBox
#endif
