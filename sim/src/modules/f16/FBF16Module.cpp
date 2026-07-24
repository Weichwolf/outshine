#include "FBF16Module.h"

namespace FlightBox {

FBF16Module::FBF16Module()
    : AP(std::make_unique<FBAutopilot>()),
      FC(std::make_unique<FBFlightControl>(FBFlightControl::F16())),
      Input(std::make_unique<FBInputSystem>()),
      Propulsion(std::make_unique<FBPropulsionSystem>()),
      Displays(std::make_unique<FBDisplaySystem>()),
      Sensors(std::make_unique<FBSensorSystem>()),
      Weapons(std::make_unique<FBWeaponSystem>()),
      Defensive(std::make_unique<FBDefensiveSystem>()),
      Comms(std::make_unique<FBCommsSystem>()) {}

bool FBF16Module::Due(double &accS, double dt, double hz) {
  accS += dt;
  double period = 1.0 / hz;
  if (accS < period) return false;
  accS -= period;
  return true;
}

/* Cycles every system slot the doc/f16/ inventory names (see the header's rate table), then the
 * fixed 100 Hz FDM substeps (spiral guard, <=12/frame): guidance -> FLCS-command -> JSBSim in
 * lockstep. AP->Run() / FC->Run() are the only virtual dispatch INSIDE that inner loop (one call
 * each per substep); every other slot below is throttled OUTSIDE it, at most once per Run(). */
void FBF16Module::Run(fb_fdm_state &st, double dt, const FBWorld *world) {
  Input->Run(Mode, dt);            /* HOTAS/ICP: once per Run() call, the coarsest sim tick */
  Propulsion->Run(st, dt);         /* engine-system logic above the raw FDM: same cadence */

  if (Due(SensorAccS, dt, 10.0)) Sensors->Run(SharedState, world, dt);
  if (Due(DisplayAccS, dt, 20.0)) Displays->Run(SharedState, Mode, dt);
  if (Due(WeaponAccS, dt, 20.0)) Weapons->Run(Mode, world, dt);
  if (Due(DefensiveAccS, dt, 5.0)) Defensive->Run(world, dt);
  if (Due(CommsAccS, dt, 1.0)) Comms->Run(dt);

  AccS += dt;
  LastSub = 0;
  for (int k = 0; AccS >= 0.01 && k < 12; k++) {
    LastG = AP->Run(st);
    FBControls c = FC->Run(LastG, st);
    fb_jsbsim_set_controls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    fb_jsbsim_step(&st);
    AccS -= 0.01;
    LastSub++;
  }
}

} // namespace FlightBox
