/* FlightBox — one-line flight telemetry, shared by the WASM app (gpu_main) and the native oracle
 * (gpu_native --fly). Emitted from the SIM TICK (not the render pass): a headless run loses the
 * render device after ~frame 2, but the sim loop lives on, so this is the only flight-fidelity signal
 * that survives. Format is the fidelity-baseline.md [agl] line, extended with the loiter-hold fields
 * (speed/bank/heading/vs/ring distance) so radius/altitude/speed hold + bank feedforward are checkable
 * from the log alone. `ground` = the DEM ASL fed to JSBSim; `fdmGnd` = what the FDM is actually
 * colliding against (proves fb_jsbsim_set_ground reached the engine, not just that it was called). */
#ifndef FB_TELEMETRY_H
#define FB_TELEMETRY_H
#include <cstdio>
#include "jsbsim_adapter.h"
#include "FBAutopilot.h"

namespace FlightBox {

static inline void FBLogAgl(const fb_fdm_state &s, const FBGuidance &g, double ground, double fdmGnd) {
  const char *mode = g.Mode == FBMode::Loiter ? "LOITER" : "MANUAL";
  double agl = s.elev - ground;
  printf("[agl] alt=%.0f agl=%.0f ground=%.0f fdmGnd=%.0f spd=%.1f cas=%.1f bank=%.1f hdg=%.0f "
         "vs=%.1f ringDist=%.0f mode=%s\n",
         s.elev, agl, ground, fdmGnd, s.speed, s.cas, s.roll, s.yaw, s.vy, g.RingDistM, mode);
  fflush(stdout);
}

} // namespace FlightBox
#endif
