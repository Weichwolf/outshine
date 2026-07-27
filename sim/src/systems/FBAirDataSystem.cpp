#include "FBAirDataSystem.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

/* vx/vy/vz sind X-Plane-lokal (+x Ost, +y auf, +z Sued) = bereits ENU, keine Geodaesie noetig. */
void FBAirDataSystem::Run(FBState &state, const fb_fdm_state &fdm, double dt) {
  (void)dt;
  FBAirDataBlock &b = state.AirData;
  b.CasKt = (float)(fdm.cas * kMsToKt);
  b.Mach = (float)fdm.mach;
  b.GLoad = (float)fdm.nz;
  if (b.GLoad > PeakG) PeakG = b.GLoad;
  b.GLoadPeak = PeakG;

  double horiz = std::sqrt(fdm.vx * fdm.vx + fdm.vz * fdm.vz);
  double track = std::atan2(fdm.vx, -fdm.vz) * kRad2Deg;   /* north = -z */
  if (track < 0.0) track += 360.0;
  b.TrackDeg = (float)track;
  b.FpaDeg = (float)(std::atan2(fdm.vy, horiz > 0.01 ? horiz : 0.01) * kRad2Deg);
  b.H.Publish(state.NowS);

  CasKt = b.CasKt; Mach = b.Mach; Nz = b.GLoad; AoaDeg = (float)fdm.alphaDeg;
}

void FBAirDataSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("casKt", "kt");
  schema.Add("mach");
  schema.Add("nz", "g");
  schema.Add("aoaDeg", "deg");
}

void FBAirDataSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push((double)CasKt);
  row.Push((double)Mach);
  row.Push((double)Nz);
  row.Push((double)AoaDeg);
}

} // namespace FlightBox
