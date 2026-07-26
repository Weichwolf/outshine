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
}

} // namespace FlightBox
