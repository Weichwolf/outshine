#include "FBFdmTelemetrySource.h"

namespace FlightBox {

void FBFdmTelemetrySource::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("lat", "deg");
  schema.Add("lon", "deg");
  schema.Add("altM", "m");
  schema.Add("aglM", "m");
  schema.Add("vsMs", "m/s");
  schema.Add("pitchDeg", "deg");
  schema.Add("rollDeg", "deg");
  schema.Add("hdgDeg", "deg");
  schema.Add("fuelLbs", "lb");
  schema.Add("gearLoadFactor");   /* peak gear strut force / aircraft weight — FBFlightMonitor's own
                                    * hard-landing ratio (kHardLandingForceFactor=3.0 trips it), logged
                                    * every tick (not just at a trip) so a landing's touchdown severity is
                                    * measurable even when it stays well clear of the K.O. threshold. */
}

void FBFdmTelemetrySource::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(Fdm.lat);
  row.Push(Fdm.lon);
  row.Push(Fdm.elev);
  row.Push(Fdm.elev - GroundAslM);
  row.Push(Fdm.vy);
  row.Push(Fdm.pitch);
  row.Push(Fdm.roll);
  row.Push(Fdm.yaw);
  row.Push(fb_jsbsim_get_fuel_total_lbs());
  double weightLbs = fb_jsbsim_get_weight_lbs();
  row.Push(weightLbs > 0.0 ? fb_jsbsim_get_max_gear_force_lbs() / weightLbs : 0.0);
}

} // namespace FlightBox
