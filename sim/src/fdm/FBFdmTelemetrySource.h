/* FlightBox — FBFdmTelemetrySource: telemetry for the FDM's own pose/rates. Lives at the adapter seam
 * (not a module) because fb_fdm_state is the adapter's own POD and the only extra input is one
 * borrowed ground-truth altitude — no module-level state is involved. Both references are borrowed:
 * the caller (the app) owns the FDM state and refreshes the ground altitude every tick; this source
 * never copies either. */
#ifndef FBFDMTELEMETRYSOURCE_H
#define FBFDMTELEMETRYSOURCE_H
#include "FBTelemetry.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

class FBFdmTelemetrySource : public FBTelemetrySource {
public:
  FBFdmTelemetrySource(const fb_fdm_state &fdm, const double &groundAslM) : Fdm(fdm), GroundAslM(groundAslM) {}

  const char *TelemetryName() const override { return "fdm"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const fb_fdm_state &Fdm;
  const double &GroundAslM;
};

} // namespace FlightBox
#endif /* FBFDMTELEMETRYSOURCE_H */
