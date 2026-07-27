#include "FBStateBusTelemetry.h"

namespace FlightBox {

/* Same order as the struct, so a reader can walk one against the other — but NOT the same LENGTH, and
 * that is a rule rather than an oversight: this source sits in the middle of every telemetry.csv ever
 * measured, so appending a name here moves every column to its right. The two blocks added after it
 * (Rwr, Cmds) therefore report their own validity as the first column of their own telemetry source,
 * which the bus registers at the END (units/FBSimUnit::StartTelemetry). A block whose head is not in
 * this list is not unobservable; it is observed one column further right. */
void FBStateBusTelemetry::DeclareTelemetry(FBTelemetrySchema &schema) const {
  const char *names[] = {"blk_platform", "blk_env", "blk_airdata", "blk_radalt", "blk_nav",
                         "blk_cruise", "blk_firecontrol", "blk_ufc", "blk_stores", "blk_airframe",
                         "blk_warn", "blk_radar", "blk_datalink", "blk_bfm"};
  for (const char *n : names) schema.Add(n);
}

void FBStateBusTelemetry::SampleTelemetry(FBTelemetryRow &row) const {
  const FBBlockHeader *heads[] = {
      &Bus_.Platform.H, &Bus_.Env.H, &Bus_.AirData.H, &Bus_.RadarAlt.H, &Bus_.Nav.H,
      &Bus_.Cruise.H, &Bus_.FireControl.H, &Bus_.Ufc.H, &Bus_.Stores.H, &Bus_.Airframe.H,
      &Bus_.Warnings.H, &Bus_.Radar.H, &Bus_.Datalink.H, &Bus_.Bfm.H};
  for (const FBBlockHeader *h : heads) row.Push((int)h->Status);
}

} // namespace FlightBox
