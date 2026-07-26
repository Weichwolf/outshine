/* FlightBox — FBNavSystem: one active steerpoint + the bullseye reference, the DEFAULT implementation
 * of a module's navigation slot (steerpoint DB is real F-16 hardware — 26..30 markpoints plus the
 * mission steerpoints per doc/f16/navigation-ils.md — this is the single-active-point placeholder every
 * module starts with). Writes bearing/elevation-angle/distance/TTG to the steerpoint and bearing/
 * distance FROM the bullseye into FBState every Run(); FBF16Hud only reads them.
 *
 * Geodesy is the SAME planar ENU approximation FBAppNative/FBAppWasm already use for home_bearing/
 * home_dist (dlat/dlon * 111320 m/deg, cos-scaled) — steerpoints are tens of nm out, not intercontinental,
 * so the flat-earth error is negligible and this stays consistent with the rest of the codebase rather
 * than introducing a second geodesy convention. Run() is the override point for a module whose nav
 * solution genuinely differs (e.g. one that fuses a real INS drift model). */
#ifndef FBNAVSYSTEM_H
#define FBNAVSYSTEM_H

#include "FBFlightPlan.h"
#include "FBState.h"
#include "FBFdm.h"

namespace FlightBox {

class FBNavSystem {
public:
  virtual ~FBNavSystem() = default;

  /* elevFt: the steerpoint's OWN ground elevation (ASL) — FBF16FireControl's slant-range input. */
  void SetSteerpoint(double lat, double lon, double elevFt) { StLat = lat; StLon = lon; StElevFt = elevFt; Have = true; }
  void SetBullseye(double lat, double lon) { BullLat = lat; BullLon = lon; HaveBull = true; }

  virtual void Run(FBState &state, const fb_fdm_state &fdm, double dt);

  /* Waypoint-advance: Akteurs-Verhalten (this system already knows steerpoints/distances, doc/mission-
   * format.md), not Runner bookkeeping — the module calls this itself (guidance's own capture), not the
   * mission orchestrator. Advances `plan`'s active waypoint once within `captureM` of it, logging
   * WP_REACHED. Returns the just-reached waypoint's plan index, or -1 if none was captured this call. */
  int AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM = 500.0);

private:
  double StLat = 0.0, StLon = 0.0, StElevFt = 0.0;
  double BullLat = 0.0, BullLon = 0.0;
  bool Have = false, HaveBull = false;
};

} // namespace FlightBox
#endif
