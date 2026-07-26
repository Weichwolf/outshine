/* FlightBox — fb-test-hard-landing: a minimal, dedicated negative-proof harness for
 * core/FBFlightMonitor's gear-force hard-landing check (RESULT HARD_LANDING). Task context: today's
 * FBPilot::Climb ALWAYS commands the gear up within a couple of seconds of any positive-rate liftoff
 * (systems/FBPilot.cpp), so a mission flown through the normal Takeoff/Climb/Route phase machine cannot
 * reach the ground with the gear still extended — every such profile instead exercises the (equally
 * real) gear-up/structure-contact path, not this one. This harness isolates the gear-EXTENDED case
 * directly: it spawns the real, vendored, read-only f16 model airborne a few metres up, gear held down,
 * and drives ONLY the generic simulated control surface (fcs cmd-norm via FBFdm::SetControls, gear via
 * FBFdm::SetGear) — no FBPilot, no FBAutopilot, nothing that writes FDM state directly (the same
 * anti-cheat contract as every other client) — so gravity plus a mild forward glide produces a genuine,
 * model-simulated hard touchdown for FBFlightMonitor to judge.
 *
 * `make test-monitor` builds this -> build/fb-test-hard-landing. Exit 0 = FBFlightMonitor tripped
 * HARD_LANDING as expected (the proof); exit 1 = it did not trip before the harness's own timeout (test
 * FAILED — the monitor missed a genuinely excessive touchdown); exit 2 = setup failure (JSBSim init). */
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include "FBSimUnit.h"   /* FBBuildFlightMonitorSample: the same monitor input a real unit feeds */
#include "FBFdmBoot.h"
#include <cstdio>
#include <memory>
#include <string>

using namespace FlightBox;

int main() {
  FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Debug);

  const double lat = 46.84335, lon = 6.91523, groundAsl = 441.0;   /* Payerne threshold, matches the
                                                                     * other missions' DEM-checked value */
  const double spawnAglM = 12.0;    /* a few metres up: enough to build real sink rate, no waypoint
                                      * chase needed */
  const double speedMs = 40.0;      /* a mild forward glide (~78 kt) — not hovering, not a dive */

  /* HeightOffsetM>0: an explicit airborne IC (FBFdmBoot.h) — NOT the ground-spawn-on-gear path
   * (FBMissionSpawnActor's ground case uses <0); FbwOverride so our own FBW (a flat Manual command
   * below), not the F-16's own FLCS, is what's driving — same override every other client uses. */
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
  fdm->Step(st);   /* prime st */

  const double dt = FBFdm::kStepS;   /* the FDM's own fixed bridge step */
  const double timeoutS = 8.0;
  double simT = 0.0;
  bool tripped = false;

  while (simT < timeoutS) {
    /* The ONLY commands this harness ever issues, both through the generic simulated control surface
     * every module/pilot uses (never a state setter): idle-ish throttle so it doesn't power back up and
     * fly away, neutral stick (let it settle into whatever attitude gravity + the model's own
     * aerodynamics produce), gear held DOWN every tick (the scenario this harness exists to test). */
    fdm->SetControls(0.0, 0.0, 0.0, 0.15);
    fdm->SetGear(1.0);   /* gear down; flap/speedbrake left at the model default */

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
