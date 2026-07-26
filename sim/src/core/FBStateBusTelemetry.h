/* FlightBox — FBStateBusTelemetry: the VALIDITY of the state bus as a measurable time series. One
 * channel per block of core/FBState, carrying that block's FBBlockStatus ordinal — 0 = invalid,
 * 1 = valid, 2 = held (core/FBBlockStatus.h).
 *
 * WHY IT EXISTS. The three-state head is only worth having if it can be checked, and the values
 * themselves cannot show it: a Held block and a Valid one carry the same numbers, which is the whole
 * point of Held. Without this source, "the datalink picture is frozen between net cycles" and "the
 * radar altimeter is dead" both look like plain data in a CSV. With it, both are one column.
 *
 * BORROWS the bus it reports on (the module's own FBState) — it owns nothing and writes nothing. */
#ifndef FBSTATEBUSTELEMETRY_H
#define FBSTATEBUSTELEMETRY_H

#include "FBState.h"
#include "FBTelemetry.h"

namespace FlightBox {

class FBStateBusTelemetry : public FBTelemetrySource {
public:
  explicit FBStateBusTelemetry(const FBState &bus) : Bus_(bus) {}
  ~FBStateBusTelemetry() override = default;

  const char *TelemetryName() const override { return "blk"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const FBState &Bus_;
};

} // namespace FlightBox
#endif
