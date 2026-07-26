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
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

class FBAirDataSystem : public FBTelemetrySource {
public:
  virtual ~FBAirDataSystem() = default;

  virtual void Run(FBState &state, const fb_fdm_state &fdm, double dt);

  const char *TelemetryName() const override { return "airdata"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  float PeakG = 1.0f;   /* running max Nz since boot — the HUD's peak-G readout */
  /* Telemetry cache: mirrors what Run() just wrote into FBState, so SampleTelemetry (no FBState in its
   * signature — sources sample their OWN last result, core/ architecture banner) has something to read. */
  float CasKt = 0.0f, Mach = 0.0f, Nz = 0.0f, AoaDeg = 0.0f;
};

} // namespace FlightBox
#endif
