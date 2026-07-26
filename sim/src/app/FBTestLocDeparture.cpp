/* FlightBox — fb-test-loc-departure: a minimal, dedicated negative-proof harness for
 * core/FBFlightMonitor's LOC/departure checks (RESULT LOC). Task context: the vanilla f16 model's own
 * FLCS provides real envelope/anti-spin protection (doc/f16/flight-controls-flcs.md — an ACCEPTED model
 * characteristic per CLAUDE.md Prinzip 5, not a defect), and FlightBox's own guidance loop
 * (FBAutopilot/FBFlightControl) is deliberately gentle — so a normal mission profile, even one abusing
 * the target-speed/altitude waypoints toward a low-speed high-alpha corner, could not push this build's
 * measurements past the monitor's sustained thresholds (see the task report). This harness isolates a
 * genuine departure entry directly: sustained full-aft-stick + full-rudder at low airspeed (a classic
 * accelerated-stall/spin provocation) — driven ONLY through the generic simulated control surface
 * (fcs cmd-norm via fb_jsbsim_set_controls), never a state setter, same anti-cheat contract as every
 * other client.
 *
 * `make test-monitor` builds this -> build/fb-test-loc-departure. Exit 0 = FBFlightMonitor tripped LOC
 * as expected; exit 1 = it did not trip before the harness's own timeout (test FAILED); exit 2 = setup
 * failure (JSBSim init). */
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBMissionBoot.h"
#include "jsbsim_adapter.h"
#include <cstdio>
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

  if (fb_jsbsim_init("vendor/jsbsim/aircraft", "f16", lat, lon, groundAsl, spawnAglM, speedMs, 0.0, 1) != 0) {
    FBLog::Error("test", "RESULT", {{"result", "SETUP_FAILED"}, {"reason", "jsbsim init"}});
    return 2;
  }
  fb_jsbsim_set_ground(groundAsl);

  FBFlightMonitor monitor;
  fb_fdm_state st{};
  fb_jsbsim_step(&st);

  const double dt = 0.01;
  const double timeoutS = 20.0;
  double simT = 0.0;
  bool tripped = false;

  while (simT < timeoutS) {
    /* Sustained full-aft-stick + full-rudder + idle throttle: the classic accelerated-stall/spin
     * provocation, through the SAME simulated fcs cmd-norm channels any guidance system uses. */
    fb_jsbsim_set_controls(0.0, -1.0, 1.0, 0.1);
    fb_jsbsim_set_aux(0.0, -1.0, -1.0);   /* gear up (airborne departure, not a ground scenario) */

    fb_jsbsim_step(&st);
    simT += dt;
    FBLog::SetTime(simT);

    if (monitor.Tick(FBBuildFlightMonitorSample(st, groundAsl), simT)) {
      tripped = true;
      break;
    }
  }

  std::string resultStr = tripped ? std::string(FBKoReasonStr(monitor.Reason())) : std::string("NO_TRIP");
  std::string detailStr = tripped ? monitor.Detail() : std::string("monitor never tripped before timeout");
  FBLog::Info("test", "RESULT", {{"result", resultStr}, {"detail", detailStr}, {"durationS", simT}});
  return tripped ? 0 : 1;
}
