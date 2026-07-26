/* FlightBox — FBMissionFile: the .fbm mission format parser (doc/mission-format.md). Pure
 * string-in/struct-out (no File I/O — the App reads the file and hands this the text, keeping core/
 * platform-neutral per the module architecture banner). One flat FBMission ties together what the
 * native mission runner (FBAppNative --mission) needs: a name for the logs, the assigned FBRunway,
 * the FBFlightPlan the keyword lines build (takeoff/land become Takeoff/Land waypoints AT the runway
 * threshold; wp lines become Enroute waypoints), and a timeout in sim-seconds. */
#ifndef FBMISSIONFILE_H
#define FBMISSIONFILE_H

#include <string>
#include "FBFlightPlan.h"
#include "FBRunway.h"

namespace FlightBox {

struct FBMission {
  std::string  Name;
  FBRunway     Runway;
  bool         HaveRunway = false;
  FBFlightPlan Plan;
  double       TimeoutS = 0.0;   /* sim-seconds until TIMEOUT; 0 = unset (a parse error, not a valid mission) */
};

/* Parses one .fbm mission (doc/mission-format.md): keyword lines, '#' end-of-line comments, blank
 * lines ignored. Returns false with a "line N: ..." message in *err (if given) on any malformed line,
 * a takeoff/land keyword before runway, or a missing name/runway/timeout — `out` is only fully valid
 * on true. */
bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err = nullptr);

} // namespace FlightBox
#endif
