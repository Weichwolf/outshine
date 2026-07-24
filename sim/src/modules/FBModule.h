/* FlightBox — FBModule: one controllable module's per-frame update. The App holds each module
 * POLYMORPHICALLY through this interface (selection is a runtime concern — today the F-16 is the one
 * registered module, but the dispatch is real, not a shortcut to it) and cycles Run() in a fixed
 * order; a module never calls a peer's Run(). A module owns its own systems (guidance/FCS/planner/
 * input/propulsion/displays/sensors/weapons/defensive/comms) and cycles them internally, each at its
 * own rate — the heterogeneous-rate scheduling is module-internal, not part of this interface. */
#ifndef FBMODULE_H
#define FBMODULE_H

#include "jsbsim_adapter.h"

namespace FlightBox {

class FBWorld;   /* borrowed only — sensors/weapons/defensive query it, the module never owns it */

class FBModule {
public:
  virtual ~FBModule() = default;

  /* Advances the module's FDM at its own fixed substep rate for `dt` wall-seconds. `st` is the
   * shared live FDM state the caller (App) reads back for camera/HUD/telemetry. `world` is a borrowed
   * reference (nullptr where a module has no world-facing systems yet) — never global access. */
  virtual void Run(fb_fdm_state &st, double dt, const FBWorld *world = nullptr) = 0;
};

} // namespace FlightBox
#endif
