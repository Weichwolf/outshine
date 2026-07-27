/* fb-test-loc-departure: the negative proof for FBFlightMonitor's LOC checks. It exists because the
 * model's own FLCS has real anti-spin protection (an ACCEPTED model characteristic) and FlightBox's
 * guidance is deliberately gentle, so no normal mission profile can push the measurements past the
 * monitor's sustained thresholds. Sustained full aft stick + full rudder at low airspeed, driven only
 * through the generic control surface. Exit 0 = it tripped, 1 = it missed one, 2 = setup failure. */
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

  const double lat = 46.84335, lon = 6.91523, groundAsl = 441.0;
  const double spawnAglM = 1200.0;   /* plenty of altitude to let a departure develop before any ground
                                      * contact could pre-empt it */
  const double speedMs = 45.0;       /* slow (~87 kt) — near the low-speed/high-alpha corner the anti-
                                      * spin rudder logic itself keys off (doc/f16: <170 kt) */

  FBFdmSpawn ic;
  ic.ModelsRoot = "assets/aircraft"; ic.Aircraft = "f16";
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
    /* The classic accelerated-stall/spin provocation, through the SAME channels any guidance uses. */
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
