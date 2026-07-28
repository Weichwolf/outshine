/* A DECLARED CYLINDER of mission geometry — a belt, a floor, a corridor — and deliberately the most
 * restricted header in the tree: it is reachable from missions/ and from FBMissionMonitor, and from
 * nowhere else. That is a NARROWING and not a widening. A pilot that could read a declared belt would
 * know where the SAMs are without a sensor, which is the one leak this whole round is shaped to
 * prevent; a module or a sensor that could read one would be reading the mission author's mind.
 *
 * It is pure geometry: a name, a centre, a radius and an altitude band. Nothing in it names a unit, a
 * team or a capability, and the judge does not check that a declared belt has any sites in it — the
 * mission author writes the geometry twice (once as `spawn`, once as `zone`) and the runner does not
 * police the agreement, exactly as it does not for the `time`/`wx` pair.
 * doc/air-defence-network.md §4. */
#ifndef FBZONE_H
#define FBZONE_H

#include <string>
#include "FBGeodesy.h"

namespace FlightBox {

struct FBZone {
  std::string Name;
  double LatDeg = 0.0, LonDeg = 0.0;
  double RadiusM = 0.0;
  double AltMinM = 0.0, AltMaxM = 0.0;   /* metres ASL, the same currency `spawn` and `wp` use */
};

/* A cylinder test in the one currency the rest of the judge already measures in: planar range plus an
 * altitude band. Deliberately not a sphere — a belt is what a defence covers over the ground. */
inline bool FBZoneContains(const FBZone &z, double latDeg, double lonDeg, double altM) {
  if (altM < z.AltMinM || altM > z.AltMaxM) return false;
  return FBPlanarDistM(latDeg, lonDeg, z.LatDeg, z.LonDeg) <= z.RadiusM;
}

/* One unit's running record for one zone. Monotone in DwellS, which is what lets `avoid zone` latch its
 * failure the way `no_fire` latches its own. */
struct FBZoneDwell {
  bool   In = false;
  double DwellS = 0.0;
};

} // namespace FlightBox
#endif
