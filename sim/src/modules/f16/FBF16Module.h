/* FlightBox — FBF16Module: the F-16, today's one registered FBModule. Owns its guidance
 * (FBAutopilot) and inner FBW (FBFlightControl) and cycles them each fixed 100 Hz substep — the App
 * loop drives the aircraft through Run() alone instead of stepping FBAutopilot/FBFlightControl
 * separately. */
#ifndef FBF16MODULE_H
#define FBF16MODULE_H

#include "FBModule.h"
#include "FBAutopilot.h"
#include "FBFlightControl.h"

namespace FlightBox {

class FBF16Module : public FBModule {
public:
  FBF16Module();

  void Run(fb_fdm_state &st, double dt) override;

  FBAutopilot &Autopilot() { return AP; }
  FBFlightControl &FlightControl() { return FC; }
  const FBGuidance &LastGuidance() const { return LastG; }
  int LastSubsteps() const { return LastSub; }

private:
  FBAutopilot AP;
  FBFlightControl FC;
  FBGuidance LastG{};
  double AccS = 0.0;
  int LastSub = 0;
};

} // namespace FlightBox
#endif
