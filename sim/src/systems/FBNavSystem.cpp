#include "FBNavSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
/* Bearing (deg true, 0..360) + horizontal distance (m) FROM (lat0,lon0) TO (lat1,lon1), one planar-ENU
 * offset shared by both (core/FBGeodesy.h — the ONE definition of that approximation). */
void BearingDist(double lat0, double lon0, double lat1, double lon1, double &bearingDeg, double &distM) {
  double e, n;
  FBEnuOffsetM(lat0, lon0, lat1, lon1, e, n);
  distM = std::sqrt(e * e + n * n);
  bearingDeg = std::atan2(e, n) * kRad2Deg;
  if (bearingDeg < 0.0) bearingDeg += 360.0;
}
} // namespace

void FBNavSystem::Run(FBState &state, const fb_fdm_state &fdm, double dt) {
  (void)dt;
  FBNavBlock &b = state.Nav;
  b.MagVarDeg = 0.0f;   /* placeholder — no declination model yet */

  if (Have) {
    double brg, distM;
    BearingDist(fdm.lat, fdm.lon, StLat, StLon, brg, distM);
    double altDiffM = StElevFt * kFtToM - fdm.elev;
    b.SteerBearingDeg = (float)brg;
    b.SteerElevAngleDeg = (float)(std::atan2(altDiffM, distM > 1.0 ? distM : 1.0) * kRad2Deg);
    b.SteerDistNm = (float)(distM * kMToNm);
    b.SteerElevFt = (float)StElevFt;
    /* The CRUS-page computed field, on its own head: with the gear down the real jet FREEZES it rather
     * than blanking it (doc/f16/controls-commands.md CRUS table), so it stops being republished and the
     * head goes Held — last value, last stamp, no new arithmetic. */
    bool gearDown = state.Airframe.H.Readable() && state.Airframe.GearPosition > 0.5f;
    if (gearDown) {
      state.Cruise.H.Hold();
    } else {
      state.Cruise.SteerTtgS = (float)(distM / (fdm.gs > 1.0 ? fdm.gs : 1.0));
      state.Cruise.H.Publish(state.NowS);
    }
  }
  if (HaveBull) {
    double brg, distM;
    BearingDist(BullLat, BullLon, fdm.lat, fdm.lon, brg, distM);   /* FROM bullseye TO aircraft */
    b.BullBearingDeg = (float)brg;
    b.BullDistNm = (float)(distM * kMToNm);
  }
  if (Have || HaveBull) b.H.Publish(state.NowS);
}

int FBNavSystem::AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM) {
  const FBWaypoint *wp = plan.ActiveWaypoint();
  if (!wp) return -1;
  /* Reference = the aircraft (its latitude scales the longitude), the historical convention here. */
  if (FBPlanarDistM(lat, lon, wp->LatDeg, wp->LonDeg) > captureM) return -1;
  int idx = plan.ActiveIndex();
  plan.SetActiveIndex(idx + 1);
  FBLog::Info("nav", "WP_REACHED", {{"idx", idx}, {"lat", wp->LatDeg}, {"lon", wp->LonDeg}});
  return idx;
}

} // namespace FlightBox
