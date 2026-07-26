#include "FBPilot.h"

namespace FlightBox {

/* Round A: every phase is a placeholder for its future procedure logic (doc/f16/procedures-*.md); only
 * the neutral output is implemented, so the switch is future hook points, not dead code — the Idle case
 * is spelled out because it is the one this round actually specifies (class banner), the rest share its
 * neutral return via default until their own logic lands. */
FBPilotCommands FBPilot::Run(const FBState &state, const fb_fdm_state &fdm, const FBFlightPlan &plan,
                             const FBRunway *runway, const FBWorld *world, double dt) {
  (void)state; (void)fdm; (void)plan; (void)runway; (void)world; (void)dt;
  switch (CurPhase) {
    case Phase::Idle:
    default:
      return FBPilotCommands{};
  }
}

} // namespace FlightBox
