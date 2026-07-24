/* FlightBox — FBModule: one controllable module's per-frame update. The App owns each FBModule (as
 * it owns any other subsystem) and cycles Run() in a fixed order; a module never calls a peer's
 * Run(). */
#ifndef FBMODULE_H
#define FBMODULE_H

#include "jsbsim_adapter.h"

namespace FlightBox {

class FBModule {
public:
  virtual ~FBModule() = default;

  /* Advances the module's FDM at its own fixed substep rate for `dt` wall-seconds. `st` is the
   * shared live FDM state the caller (App) reads back for camera/HUD/telemetry. */
  virtual void Run(fb_fdm_state &st, double dt) = 0;
};

} // namespace FlightBox
#endif
