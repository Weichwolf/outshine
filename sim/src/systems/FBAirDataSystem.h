/* FlightBox — FBAirDataSystem: ADC-class air data (CAS/Mach/G) plus the FPM's world-referenced
 * direction. doc/flightbox/systems.md, Abschnitt 4. */
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
  float PeakG = 1.0f;   /* running max Nz since boot */
  /* Telemetry cache: SampleTelemetry has no FBState in its signature — a source samples its OWN result. */
  float CasKt = 0.0f, Mach = 0.0f, Nz = 0.0f, AoaDeg = 0.0f;
};

} // namespace FlightBox
#endif
