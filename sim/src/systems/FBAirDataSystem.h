/* FlightBox — FBAirDataSystem: ADC-class air data (CAS/Mach/G) plus the flight path marker's
 * world-referenced direction, the DEFAULT implementation of a module's air-data slot. Airframe-agnostic
 * — every module with an ADC and a velocity vector gets the same numbers; an airframe whose air-data
 * chain genuinely differs overrides Run(), same override-point pattern as FBAutopilot/FBFlightControl.
 *
 * FPM direction as WORLD az/el (ground track + flight-path angle from the velocity vector), not a
 * body-relative offset: FBF16Hud projects it through the SAME camera basis (yaw/pitch/roll) the
 * conformal horizon already uses, so attitude does not have to be composed twice. */
#ifndef FBAIRDATASYSTEM_H
#define FBAIRDATASYSTEM_H

#include "FBState.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

class FBAirDataSystem {
public:
  virtual ~FBAirDataSystem() = default;

  virtual void Run(FBState &state, const fb_fdm_state &fdm, double dt);

private:
  float PeakG = 1.0f;   /* running max Nz since boot — the HUD's peak-G readout */
};

} // namespace FlightBox
#endif
