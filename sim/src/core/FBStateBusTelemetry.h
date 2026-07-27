/* The VALIDITY of the state bus as a measurable time series: one channel per FBState block, carrying
 * its FBBlockStatus ordinal. It exists because the VALUES cannot show it — a Held block carries the
 * same numbers as a Valid one, which is the whole point of Held. Borrows the bus, owns nothing.
 * doc/flightbox/core.md, Abschnitt 1.6. */
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
