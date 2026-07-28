/* fb-test-hard-landing: the negative proof for FBFlightMonitor's gear-force check. It exists because
 * FBPilot::Climb always retracts the gear seconds after liftoff, so no mission flown through the normal
 * phase machine can reach the ground gear-EXTENDED — every such profile exercises the structure-contact
 * path instead. Drives only the generic simulated controls, never a state setter, so the touchdown is
 * genuinely model-produced. Exit 0 = it tripped (the proof), 1 = it missed one, 2 = setup failure. */
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
  Clients::FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Debug);

  const double lat = 46.84335, lon = 6.91523, groundAsl = 441.0;   /* Payerne threshold, matches the
                                                                     * other missions' DEM-checked value */
  const double spawnAglM = 12.0;    /* a few metres up: enough to build real sink rate, no waypoint
                                      * chase needed */
  const double speedMs = 40.0;      /* a mild forward glide (~78 kt) — not hovering, not a dive */

  /* HeightOffsetM > 0 is the explicit AIRBORNE IC, not the ground-spawn-on-gear path (which uses < 0). */
  Fdm::FBFdmSpawn ic;
  ic.ModelsRoot = "assets/aircraft"; ic.Aircraft = "f16";
  ic.LatDeg = lat; ic.LonDeg = lon; ic.GroundElevM = groundAsl; ic.HeightOffsetM = spawnAglM;
  ic.SpeedMs = speedMs; ic.HeadingDeg = 0.0; ic.FbwOverride = true;
  std::unique_ptr<Fdm::FBFdm> fdm = Fdm::FBFdmBoot::Spawn(ic);
  if (!fdm) {
    FBLog::Error("test", "RESULT", {{"result", "SETUP_FAILED"}, {"reason", "jsbsim init"}});
    return 2;
  }
  fdm->SetGroundElevM(groundAsl);

  FBFlightMonitor monitor;
  Fdm::fb_fdm_state st{};
  fdm->Step(st);   /* prime st */

  const double dt = Fdm::FBFdm::kStepS;   /* the FDM's own fixed bridge step */
  const double timeoutS = 8.0;
  double simT = 0.0;
  bool tripped = false;

  while (simT < timeoutS) {
    /* The ONLY commands this harness ever issues, all through the generic control surface and never a
     * state setter: idle throttle so it cannot fly away, neutral stick so the attitude is the model's
     * own, gear held DOWN — the scenario itself. */
    fdm->SetControls(0.0, 0.0, 0.0, 0.15);
    fdm->SetGear(1.0);   /* gear down; flap/speedbrake left at the model default */

    fdm->Step(st);
    simT += dt;
    FBLog::SetTime(simT);

    if (monitor.Tick(Units::FBBuildFlightMonitorSample(*fdm, st, groundAsl), simT)) {
      tripped = true;
      break;
    }
  }

  std::string resultStr = tripped ? std::string(FBKoReasonStr(monitor.Reason())) : std::string("NO_TRIP");
  std::string detailStr = tripped ? monitor.Detail() : std::string("monitor never tripped before timeout");
  FBLog::Info("test", "RESULT", {{"result", resultStr}, {"detail", detailStr}, {"durationS", simT}});
  return tripped ? 0 : 1;
}
