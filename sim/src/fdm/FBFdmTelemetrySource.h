/* FlightBox — FBFdmTelemetrySource: telemetry for the FDM's own pose/rates. Lives at the adapter seam
 * (not a module) because fb_fdm_state is the FDM's own POD and the only extra inputs are the airframe
 * it came from and one borrowed ground-truth altitude — no module-level state is involved. All three
 * references are borrowed and constructor-injected: this source is created per run, AFTER the FDM
 * exists, and stays bound to that one airframe for its whole life. `const FBFdm*` is a read-only handle
 * (FBFdm.h) — telemetry observes, it never commands.
 *
 * THE AIRFRAME IS OPTIONAL, and the pointer says so: a unit without one (units/FBSimUnit's banner — a
 * static ground target) still has a pose, an altitude and a ground sample, which is most of this
 * schema. The two columns that are genuinely the AIRFRAME's — its fuel and the load its gear is taking —
 * are the only ones that go to zero, and the column set stays identical, so every trace in a run has the
 * same header whatever kind of unit produced it. */
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
  const FBFdm *Fdm;   /* null: a unit with no airframe (see the banner) */
  const fb_fdm_state &St;
  const double &GroundAslM;
};

} // namespace FlightBox
#endif /* FBFDMTELEMETRYSOURCE_H */
