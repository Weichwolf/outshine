#include "FBStateBusTelemetry.h"

namespace FlightBox {

void FBStateBusTelemetry::DeclareTelemetry(FBTelemetrySchema &schema) const {
  /* Same order as the struct, so a reader can walk one against the other. */
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
