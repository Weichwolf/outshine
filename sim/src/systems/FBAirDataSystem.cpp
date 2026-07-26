#include "FBAirDataSystem.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr double kMsToKt = 1.9438444924406;
constexpr double kR2D = 57.29577951308232;
} // namespace

/* CAS/Mach/G straight off the FDM; ground track + flight-path angle from the X-Plane-local velocity
 * (+x east, +y up, +z south — already the local ENU frame, no geodesy needed). */
void FBAirDataSystem::Run(FBState &state, const fb_fdm_state &fdm, double dt) {
  (void)dt;
  state.casKts = (float)(fdm.cas * kMsToKt);
  state.mach = (float)fdm.mach;
  state.gLoad = (float)fdm.nz;
  if (state.gLoad > PeakG) PeakG = state.gLoad;
  state.gLoadPeak = PeakG;

  double horiz = std::sqrt(fdm.vx * fdm.vx + fdm.vz * fdm.vz);
  double track = std::atan2(fdm.vx, -fdm.vz) * kR2D;   /* north = -z */
  if (track < 0.0) track += 360.0;
  state.trackDeg = (float)track;
  state.fpaDeg = (float)(std::atan2(fdm.vy, horiz > 0.01 ? horiz : 0.01) * kR2D);

  CasKt = state.casKts; Mach = state.mach; Nz = state.gLoad; AoaDeg = (float)fdm.alphaDeg;
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
