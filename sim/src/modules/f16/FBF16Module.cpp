#include "FBF16Module.h"

namespace FlightBox {

FBF16Module::FBF16Module() : FC(FBFlightControl::F16()) {}

/* Fixed 100 Hz substeps with a spiral guard (<=12/frame): guidance -> FLCS-command -> JSBSim in
 * lockstep. */
void FBF16Module::Run(fb_fdm_state &st, double dt) {
  AccS += dt;
  LastSub = 0;
  for (int k = 0; AccS >= 0.01 && k < 12; k++) {
    LastG = AP.Run(st);
    FBControls c = FC.Run(LastG, st);
    fb_jsbsim_set_controls(c.Roll, c.Pitch, c.Yaw, c.Thr);
    fb_jsbsim_step(&st);
    AccS -= 0.01;
    LastSub++;
  }
}

} // namespace FlightBox
