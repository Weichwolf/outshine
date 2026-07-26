#include "FBNavSystem.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr double kMPerDeg = 111320.0;
constexpr double kR2D = 57.29577951308232;
constexpr double kD2R = 3.14159265358979323846 / 180.0;
constexpr double kMToNm = 1.0 / 1852.0;
constexpr double kFtToM = 0.3048;

/* Planar ENU bearing (deg true, 0..360) + horizontal distance (m) FROM (lat0,lon0) TO (lat1,lon1) —
 * same dlat/dlon*111320 approximation as FBAppNative/FBAppWasm's home_bearing/home_dist. */
void BearingDist(double lat0, double lon0, double lat1, double lon1, double &bearingDeg, double &distM) {
  double coslat = std::cos(lat0 * kD2R);
  double dlon = lon1 - lon0;
  while (dlon > 180.0) dlon -= 360.0;
  while (dlon < -180.0) dlon += 360.0;
  double y = (lat1 - lat0) * kMPerDeg;
  double x = dlon * kMPerDeg * coslat;
  distM = std::sqrt(x * x + y * y);
  bearingDeg = std::atan2(x, y) * kR2D;
  if (bearingDeg < 0.0) bearingDeg += 360.0;
}
} // namespace

void FBNavSystem::Run(FBState &state, const fb_fdm_state &fdm, double dt) {
  (void)dt;
  state.magVarDeg = 0.0f;   /* placeholder — no declination model yet */

  if (Have) {
    double brg, distM;
    BearingDist(fdm.lat, fdm.lon, StLat, StLon, brg, distM);
    double altDiffM = StElevFt * kFtToM - fdm.elev;
    state.steerBearingDeg = (float)brg;
    state.steerElevAngleDeg = (float)(std::atan2(altDiffM, distM > 1.0 ? distM : 1.0) * kR2D);
    state.steerDistNm = (float)(distM * kMToNm);
    state.steerElevFt = (float)StElevFt;
    state.steerTtgS = (float)(distM / (fdm.gs > 1.0 ? fdm.gs : 1.0));
  }
  if (HaveBull) {
    double brg, distM;
    BearingDist(BullLat, BullLon, fdm.lat, fdm.lon, brg, distM);   /* FROM bullseye TO aircraft */
    state.bullBearingDeg = (float)brg;
    state.bullDistNm = (float)(distM * kMToNm);
  }
}

int FBNavSystem::AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM) {
  const FBWaypoint *wp = plan.ActiveWaypoint();
  if (!wp) return -1;
  double coslat = std::cos(lat * kD2R);
  double dy = (lat - wp->LatDeg) * kMPerDeg, dx = (lon - wp->LonDeg) * kMPerDeg * coslat;
  if (std::sqrt(dx * dx + dy * dy) > captureM) return -1;
  int idx = plan.ActiveIndex();
  plan.SetActiveIndex(idx + 1);
  FBLog::Info("nav", "WP_REACHED", {{"idx", idx}, {"lat", wp->LatDeg}, {"lon", wp->LonDeg}});
  return idx;
}

} // namespace FlightBox
