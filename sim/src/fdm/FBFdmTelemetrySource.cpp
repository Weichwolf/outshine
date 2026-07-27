#include "FBFdmTelemetrySource.h"

namespace FlightBox::Fdm {

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
  /* FBFlightMonitor's own hard-landing ratio, logged EVERY tick so touchdown severity is measurable
   * well below the K.O. threshold. */
  schema.Add("gearLoadFactor");
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
  row.Push(Fdm ? Fdm->GetFuelTotalLbs() : 0.0);
  double weightLbs = Fdm ? Fdm->GetWeightLbs() : 0.0;
  row.Push(weightLbs > 0.0 ? Fdm->GetMaxGearForceLbs() / weightLbs : 0.0);
}

} // namespace FlightBox::Fdm
