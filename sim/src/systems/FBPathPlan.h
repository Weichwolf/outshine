/* FlightBox — FBPathPlan: the LOWLEVEL lateral path planner. A-star over the coarse DEM
 * (FBTerrainField) with a cost that penalises terrain HEIGHT, so the minimum-cost route automatically
 * follows valleys and crosses ridges at their lowest pass (never "avoid the Alps", always "take the
 * lowest gap" — the user's brief). Wander goals are drawn deterministically (seedable) inside a fence
 * around home; on arrival the next goal is planned -> perpetual flight. The grid path is simplified to a
 * flyable waypoint chain; the app steers the autopilot's heading by pure-pursuit to the active waypoint.
 * Planning is synchronous here (the native oracle): a replan is a one-off per goal, and the sim is
 * wall-clock-free, so a sub-second A* does not perturb the flight result.
 *
 * Update() is virtual: the DEFAULT here is airframe-agnostic wander/valley-following; a module with a
 * genuinely different planning law (not just different cell size/weights, which are the public
 * tunables below) overrides it. */
#ifndef FBPATHPLAN_H
#define FBPATHPLAN_H

#include <cstdint>
#include <vector>
#include <utility>

namespace FlightBox {

class FBTerrainField;

class FBPathPlan {
public:
  FBPathPlan(FBTerrainField *field, double centerLat, double centerLon, double fenceRadiusM, unsigned seed);
  virtual ~FBPathPlan() = default;

  void SetFixedGoal(double lat, double lon);   /* proof override: one fixed goal instead of random wander */

  /* Call each frame with the live position: on first call / on goal arrival it (re)plans, and it advances
   * the active waypoint as the aircraft passes. Cheap when the route is stable (just a capture check).
   * The one override point (see the class banner). */
  virtual void Update(double lat, double lon);

  bool   HasRoute(void) const { return Route.size() >= 2; }
  void   Reroute(double lat, double lon) { Plan(lat, lon); }   /* force a re-plan to the current goal (refresh) */
  double DesiredTrackDeg(double lat, double lon) const;   /* pure-pursuit bearing to the active waypoint */

  /* Telemetry for the [plan] line. */
  double GoalLat(void) const { return GLat; }
  double GoalLon(void) const { return GLon; }
  int    ActiveWp(void) const { return Active; }
  int    RouteSize(void) const { return (int)Route.size(); }
  double GoalDistM(double lat, double lon) const;
  long   Replans(void) const { return ReplanCount; }
  long   LastExpanded(void) const { return LastExpandedN; }

  /* The planned waypoints (lat,lon), for the track-on-relief proof plot. */
  const std::vector<std::pair<double, double>> &Waypoints(void) const { return Route; }

  /* Tunables (public config block). */
  double CellM;        /* planning cell size (m) */
  double HeightW;      /* terrain-height cost weight: cost/m = 1 + HeightW * height/1000 */
  double CaptureM;     /* waypoint capture radius (~ turn radius at cruise) */
  double FencePad;     /* keep the path this far (m) inside the fence via a soft penalty */
  long   MaxExpand;    /* A* expansion budget (safety cap) */

private:
  void NewGoal(void);
  bool Plan(double slat, double slon);   /* A* from (slat,slon) to the goal; fills Route (simplified) */

  FBTerrainField *Field;
  double CLat, CLon, FenceM;
  uint32_t Rng;
  double GLat, GLon;
  bool HaveGoal, Fixed;
  std::vector<std::pair<double, double>> Route;   /* [0] = plan start, ... goal */
  int Active;
  long ReplanCount, LastExpandedN;
};

} // namespace FlightBox
#endif
