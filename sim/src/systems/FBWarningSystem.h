/* FlightBox — FBWarningSystem: der Warnsatz als Bitmaske; jede Bedingung ist eine Fusion fremder
 * Bloecke und kann daher UNAUSWERTBAR sein (Inhibited). doc/flightbox/systems.md, Abschnitt 6. */
#ifndef FBWARNINGSYSTEM_H
#define FBWARNINGSYSTEM_H

#include "FBState.h"
#include "FBTelemetry.h"

namespace FlightBox::Systems {

class FBWarningSystem : public FBTelemetrySource {
public:
  ~FBWarningSystem() override = default;

  virtual void Run(FBState &state, double dt);

  const char *TelemetryName() const override { return "warn"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  uint32_t Active_ = 0, Inhibited_ = 0;   /* Telemetriekopie des zuletzt publizierten Blocks */
};

} // namespace FlightBox::Systems
#endif
