/* FlightBox — FBWarningSystem: the caution/warning set, the module slot that turns other systems'
 * published blocks into the annunciator bitmask (core/FBAvionicsBlocks.h's FBWarningBlock). Generic
 * and airframe-agnostic like the rest of systems/; Run() is the override point for an airframe whose
 * warning logic genuinely differs.
 *
 * IT EXISTS TO MAKE THE VALIDITY HEADS CONSEQUENTIAL. Every condition here is a fusion of blocks
 * written elsewhere, and each one can therefore be UNEVALUABLE — which is a third answer, distinct from
 * "warning" and "no warning", and the block carries it separately (Inhibited). The reference case is
 * documented, not invented: the CARA ALOW warning fires only with the radar altimeter powered and
 * transmitting (doc/f16/controls-commands.md §6.4) — the DED accepts the threshold either way, and the
 * *effect* is what the sensor gates. So an Invalid radar-altitude block does not mean "not low"; it
 * means nobody can say, and the annunciator says exactly that.
 *
 * WRITES the warning block and nothing else; READS radar altitude, the UFC's committed thresholds and
 * the airframe block. It commands nothing — a warning system that could act would be a second pilot. */
#ifndef FBWARNINGSYSTEM_H
#define FBWARNINGSYSTEM_H

#include "FBState.h"
#include "FBTelemetry.h"

namespace FlightBox {

class FBWarningSystem : public FBTelemetrySource {
public:
  ~FBWarningSystem() override = default;

  virtual void Run(FBState &state, double dt);

  const char *TelemetryName() const override { return "warn"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  uint32_t Active_ = 0, Inhibited_ = 0;   /* telemetry's copy of the last published block */
};

} // namespace FlightBox
#endif
