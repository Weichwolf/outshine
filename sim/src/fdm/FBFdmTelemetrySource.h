/* FlightBox — FBFdmTelemetrySource: telemetry for the FDM's own pose/rates, at the adapter seam because
 * fb_fdm_state is the FDM's POD. The airframe is OPTIONAL (a unit without one still has a pose); only
 * the two genuinely airframe-owned columns go to zero, so every trace keeps the same header. */
#ifndef FBFDMTELEMETRYSOURCE_H
#define FBFDMTELEMETRYSOURCE_H
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

class FBFdmTelemetrySource : public FBTelemetrySource {
public:
  FBFdmTelemetrySource(const FBFdm *fdm, const fb_fdm_state &st, const double &groundAslM)
      : Fdm(fdm), St(st), GroundAslM(groundAslM) {}

  const char *TelemetryName() const override { return "fdm"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const FBFdm *Fdm;   /* null: a unit with no airframe */
  const fb_fdm_state &St;
  const double &GroundAslM;
};

} // namespace FlightBox
#endif /* FBFDMTELEMETRYSOURCE_H */
