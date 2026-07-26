/* FlightBox — fb-test-loc-departure: a minimal, dedicated negative-proof harness for
 * core/FBFlightMonitor's LOC/departure checks (RESULT LOC). Task context: the vanilla f16 model's own
 * FLCS provides real envelope/anti-spin protection (doc/f16/flight-controls-flcs.md — an ACCEPTED model
 * characteristic per CLAUDE.md Prinzip 5, not a defect), and FlightBox's own guidance loop
 * (FBAutopilot/FBFlightControl) is deliberately gentle — so a normal mission profile, even one abusing
 * the target-speed/altitude waypoints toward a low-speed high-alpha corner, could not push this build's
 * measurements past the monitor's sustained thresholds (see the task report). This harness isolates a
 * genuine departure entry directly: sustained full-aft-stick + full-rudder at low airspeed (a classic
 * accelerated-stall/spin provocation) — driven ONLY through the generic simulated control surface
 * (fcs cmd-norm via FBFdm::SetControls), never a state setter, same anti-cheat contract as every
 * other client.
 *
 * `make test-monitor` builds this -> build/fb-test-loc-departure. Exit 0 = FBFlightMonitor tripped LOC
 * as expected; exit 1 = it did not trip before the harness's own timeout (test FAILED); exit 2 = setup
 * failure (JSBSim init). */
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBMissionBoot.h"
#include "FBFdmBoot.h"
#include <cstdio>
#include <memory>
#include <string>

using namespace FlightBox;

int main() {
  FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Debug);

  const double lat = 46.84335, lon = 6.91523, groundAsl = 441.0;
  const double spawnAglM = 1200.0;   /* plenty of altitude to let a departure develop before any ground
                                      * contact could pre-empt it */
  const double speedMs = 45.0;       /* slow (~87 kt) — near the low-speed/high-alpha corner the anti-
                                      * spin rudder logic itself keys off (doc/f16: <170 kt) */

  FBFdmSpawn ic;
  ic.ModelsRoot = "vendor/jsbsim/aircraft"; ic.Aircraft = "f16";
  ic.LatDeg = lat; ic.LonDeg = lon; ic.GroundElevM = groundAsl; ic.HeightOffsetM = spawnAglM;
  ic.SpeedMs = speedMs; ic.HeadingDeg = 0.0; ic.FbwOverride = true;
  std::unique_ptr<FBFdm> fdm = FBFdmBoot::Spawn(ic);
  if (!fdm) {
    FBLog::Error("test", "RESULT", {{"result", "SETUP_FAILED"}, {"reason", "jsbsim init"}});
    return 2;
  }
  fdm->SetGroundElevM(groundAsl);

  FBFlightMonitor monitor;
  fb_fdm_state st{};
  fdm->Step(st);

  const double dt = FBFdm::kStepS;
  const double timeoutS = 20.0;
  double simT = 0.0;
  bool tripped = false;

  while (simT < timeoutS) {
    /* Sustained full-aft-stick + full-rudder + idle throttle: the classic accelerated-stall/spin
     * provocation, through the SAME simulated fcs cmd-norm channels any guidance system uses. */
    fdm->SetControls(0.0, -1.0, 1.0, 0.1);
    fdm->SetGear(0.0);   /* gear up (airborne departure, not a ground scenario) */

    fdm->Step(st);
    simT += dt;
    FBLog::SetTime(simT);

    if (monitor.Tick(FBBuildFlightMonitorSample(*fdm, st, groundAsl), simT)) {
      tripped = true;
      break;
    }
  }

  std::string resultStr = tripped ? std::string(FBKoReasonStr(monitor.Reason())) : std::string("NO_TRIP");
  std::string detailStr = tripped ? monitor.Detail() : std::string("monitor never tripped before timeout");
  FBLog::Info("test", "RESULT", {{"result", resultStr}, {"detail", detailStr}, {"durationS", simT}});
  return tripped ? 0 : 1;
}
