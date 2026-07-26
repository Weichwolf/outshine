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
  row.Push(St.lat);
  row.Push(St.lon);
  row.Push(St.elev);
  row.Push(St.elev - GroundAslM);
  row.Push(St.vy);
  row.Push(St.pitch);
  row.Push(St.roll);
  row.Push(St.yaw);
  row.Push(Fdm.GetFuelTotalLbs());
  double weightLbs = Fdm.GetWeightLbs();
  row.Push(weightLbs > 0.0 ? Fdm.GetMaxGearForceLbs() / weightLbs : 0.0);
}

} // namespace FlightBox
