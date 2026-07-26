/* FlightBox — FBMissionBoot: the ground-spawn + waypoint-advance BOTH entry points need to start a
 * mission-with-pilot run (the native --mission runner and the WASM app's default boot), factored once
 * so neither App duplicates JSBSim IC / FBPilot-arming / FBFlightPlan mechanics. Generic over FBModule
 * (mission.ModuleName picks the concrete module via FBModuleRegistry — see FBMissionRunner.cpp), not
 * hardcoded to the F-16: this header itself never names a concrete module type. Header-only: every
 * caller already links FBMissionFile.cpp + the module registry, so no new Makefile translation unit is
 * needed for this. */
#ifndef FBMISSIONBOOT_H
#define FBMISSIONBOOT_H

#include <cmath>
#include "FBMissionFile.h"
#include "FBModule.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

/* Ground-spawns `mission`'s module on its runway threshold: JSBSim IC sat-on-gear at `groundAsl`
 * (hoff_m=-1, see the adapter banner) for the JSBSim aircraft named `mission.ModuleName` (module name
 * == JSBSim aircraft directory name, e.g. "f16"), FBAutopilot neutral Manual (idle stick/throttle — a
 * real-FLCS airframe like the F-16 holds wings-level on its own) so FBPilot::Preflight's brief hold has
 * something stable to sit in, gear down + both wheel brakes set, the mission's FlightPlan/Runway wired
 * onto the module, FBPilot armed at Preflight. `aircraftPath` differs per link target (native/gym:
 * "vendor/jsbsim/aircraft", WASM: the embedded FS path "/jsbsim/aircraft"). Returns false (JSBSim init
 * failed) without touching module/st. */
inline bool FBMissionGroundSpawn(const char *aircraftPath, const FBMission &mission, double groundAsl,
                                 FBModule &module, fb_fdm_state &st) {
  const FBRunway &rwy = mission.Runway;
  if (fb_jsbsim_init(aircraftPath, mission.ModuleName.c_str(), rwy.ThresholdLatDeg, rwy.ThresholdLonDeg,
                     groundAsl, -1.0, 0.0, rwy.TrueHeadingDeg, 0) != 0)
    return false;
  fb_jsbsim_set_ground(groundAsl);
  module.SetRunway(rwy);
  module.FlightPlan() = mission.Plan;
  module.Autopilot().SetManual(0.0, 0.0, 0.0, 0.0);
  module.Controls().SetGear(true);
  module.Controls().SetWheelBrakes(1.0, 1.0);
  module.PilotSystem().SetPhase(FBPilot::Phase::Preflight);
  st = fb_fdm_state{};
  st.lat = rwy.ThresholdLatDeg; st.lon = rwy.ThresholdLonDeg; st.elev = groundAsl;
  return true;
}

/* Advances `plan`'s active waypoint once the aircraft is within `captureM` of it — the mechanical half
 * of waypoint sequencing FBPilot::Run relies on but does not itself perform (its Climb/Route phases
 * just fly whatever ActiveWaypoint() returns). Returns the just-reached waypoint's index, or -1 if
 * none was captured this call. */
inline int FBMissionAdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM) {
  const FBWaypoint *wp = plan.ActiveWaypoint();
  if (!wp) return -1;
  double coslat = std::cos(lat * 3.14159265358979323846 / 180.0);
  double dy = (lat - wp->LatDeg) * 111320.0, dx = (lon - wp->LonDeg) * 111320.0 * coslat;
  if (std::sqrt(dx * dx + dy * dy) > captureM) return -1;
  int idx = plan.ActiveIndex();
  plan.SetActiveIndex(idx + 1);
  return idx;
}

} // namespace FlightBox
#endif
