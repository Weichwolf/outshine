/* The pilot's mission brief: a plain ordered waypoint chain, structure only — procedure logic is
 * FBPilot's phase machine, not this container. */
#ifndef FBFLIGHTPLAN_H
#define FBFLIGHTPLAN_H

#include <vector>

#include "FBElevationProvider.h"

namespace FlightBox {

enum class FBWaypointType { Takeoff, Enroute, Approach, Land };

struct FBWaypoint {
  double LatDeg = 0.0, LonDeg = 0.0;
  double AltM = 0.0;      /* target altitude, m ASL */
  double SpeedKt = 0.0;   /* target speed, kt (CAS) */
  FBWaypointType Type = FBWaypointType::Enroute;
  /* The TERRAIN under the fix, which is a different number from AltM (the altitude to fly) and the one
   * a ballistic delivery solves against. Unresolved until the plan is briefed. */
  double GroundElevM = kFBElevationUnresolved;
};

class FBFlightPlan {
public:
  void AddWaypoint(const FBWaypoint &wp) { Waypoints.push_back(wp); }
  void Clear() { Waypoints.clear(); Active = 0; }

  /* THE BRIEFING'S MAP, read ONCE at spawn and never in flight: a steerpoint elevation is data the
   * pilot was handed, while a computer able to sample the ground at an arbitrary point would be a
   * knowledge source no sensor paid for. Hence the mutation lives here and not behind a mutable
   * accessor — there is exactly one moment a waypoint may learn its terrain. */
  void BriefGroundElevation(const FBElevationProvider &elev, double fallbackM) {
    for (FBWaypoint &wp : Waypoints) {
      double m = elev.GroundElevM(wp.LatDeg, wp.LonDeg);
      wp.GroundElevM = FBElevationResolved(m) ? m : fallbackM;
    }
  }

  int  Size() const { return (int)Waypoints.size(); }
  bool Empty() const { return Waypoints.empty(); }
  const FBWaypoint &At(int i) const { return Waypoints[(size_t)i]; }

  int  ActiveIndex() const { return Active; }
  void SetActiveIndex(int i) { Active = i; }
  const FBWaypoint *ActiveWaypoint() const {
    return (Active >= 0 && Active < (int)Waypoints.size()) ? &Waypoints[(size_t)Active] : nullptr;
  }

private:
  std::vector<FBWaypoint> Waypoints;
  int Active = 0;
};

} // namespace FlightBox
#endif
