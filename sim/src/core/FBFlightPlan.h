/* FlightBox — FBFlightPlan: the pilot's mission brief, a plain ordered waypoint chain. Lives in core/
 * (value type, zeroes no dependency on systems/modules) so both FBPilot (systems/) and a future
 * mission-setup UI can share it without either owning the other. Holds structure only — no procedure
 * logic (SIDs/holds/approach sequencing are FBPilot's phase machine, not this container). */
#ifndef FBFLIGHTPLAN_H
#define FBFLIGHTPLAN_H

#include <vector>

namespace FlightBox {

enum class FBWaypointType { Takeoff, Enroute, Approach, Land };

struct FBWaypoint {
  double LatDeg = 0.0, LonDeg = 0.0;
  double AltM = 0.0;      /* target altitude, m ASL */
  double SpeedKt = 0.0;   /* target speed, kt (CAS) */
  FBWaypointType Type = FBWaypointType::Enroute;
};

class FBFlightPlan {
public:
  void AddWaypoint(const FBWaypoint &wp) { Waypoints.push_back(wp); }
  void Clear() { Waypoints.clear(); Active = 0; }

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
