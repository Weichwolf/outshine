/* FlightBox — FBF16Module: the F-16, today's one registered FBModule. Composes the DEFAULT guidance
 * (systems/FBAutopilot, unmodified) with the F-16 gain preset (FBFlightControl::F16()) and cycles
 * them each fixed 100 Hz substep — the App loop drives the aircraft through Run() alone instead of
 * stepping FBAutopilot/FBFlightControl separately. */
#ifndef FBF16MODULE_H
#define FBF16MODULE_H

#include <memory>
#include "FBModule.h"
#include "FBAutopilot.h"
#include "FBFlightControl.h"

namespace FlightBox {

class FBF16Module : public FBModule {
public:
  FBF16Module();

  void Run(fb_fdm_state &st, double dt) override;

  FBAutopilot &Autopilot() { return *AP; }
  FBFlightControl &FlightControl() { return FC; }
  const FBGuidance &LastGuidance() const { return LastG; }
  int LastSubsteps() const { return LastSub; }

private:
  /* Owned through the base pointer (not a value member) so a future module can substitute a
   * guidance override (FBAutopilot::Run is the virtual point) without slicing; the F-16 composes
   * the unmodified DEFAULT. FBFlightControl stays a value member: its F16() preset is config, no
   * override exists, so no indirection is needed. */
  std::unique_ptr<FBAutopilot> AP;
  FBFlightControl FC;
  FBGuidance LastG{};
  double AccS = 0.0;
  int LastSub = 0;
};

} // namespace FlightBox
#endif
