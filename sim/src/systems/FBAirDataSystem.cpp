#include "FBAirDataSystem.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

/* CAS/Mach/G straight off the FDM; ground track + flight-path angle from the X-Plane-local velocity
 * (+x east, +y up, +z south — already the local ENU frame, no geodesy needed). */
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
