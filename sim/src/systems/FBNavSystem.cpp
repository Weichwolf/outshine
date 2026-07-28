#include "FBNavSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox::Systems {

namespace {
/* Peilung (deg rechtweisend) + Horizontaldistanz (m) VON (lat0,lon0) NACH (lat1,lon1). */
void BearingDist(double lat0, double lon0, double lat1, double lon1, double &bearingDeg, double &distM) {
  double e, n;
  FBEnuOffsetM(lat0, lon0, lat1, lon1, e, n);
  distM = std::sqrt(e * e + n * n);
  bearingDeg = std::atan2(e, n) * kRad2Deg;
  if (bearingDeg < 0.0) bearingDeg += 360.0;
}
} // namespace

void FBNavSystem::Run(FBState &state, const Fdm::fb_fdm_state &fdm, double dt) {
  (void)dt;
  FBNavBlock &b = state.Nav;
  b.MagVarDeg = 0.0f;   /* Platzhalter — kein Deklinationsmodell */

  if (Have) {
    double brg, distM;
    BearingDist(fdm.lat, fdm.lon, StLat, StLon, brg, distM);
    double altDiffM = StElevFt * kFtToM - fdm.elev;
    b.SteerBearingDeg = (float)brg;
    b.SteerElevAngleDeg = (float)(std::atan2(altDiffM, distM > 1.0 ? distM : 1.0) * kRad2Deg);
    b.SteerDistNm = (float)(distM * kMToNm);
    b.SteerElevFt = (float)StElevFt;
    /* Bei ausgefahrenem Fahrwerk FRIERT der echte Jet das CRUS-Rechenfeld EIN statt es zu leeren
     * (doc/modules/f16/controls-commands.md, CRUS-Tabelle) — daher Held statt Invalid. */
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
    BearingDist(BullLat, BullLon, fdm.lat, fdm.lon, brg, distM);   /* VOM Bullseye ZUM Flugzeug */
    b.BullBearingDeg = (float)brg;
    b.BullDistNm = (float)(distM * kMToNm);
  }
  if (Have || HaveBull) b.H.Publish(state.NowS);
}

int FBNavSystem::AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM) {
  const FBWaypoint *wp = plan.ActiveWaypoint();
  if (!wp) return -1;
  int idx = plan.ActiveIndex();
  bool reached = FBPlanarDistM(lat, lon, wp->LatDeg, wp->LonDeg) <= captureM;
  const char *by = "capture";
  /* ...ODER er liegt schlicht HINTER uns: ein Fangkreis kann einen Fix nicht beantworten, den das
   * Flugzeug physisch nie erreicht (Fix innerhalb des eigenen Kurvenradius). Der erste Wegpunkt hat
   * keine einlaufende Bahn, also gibt es dort kein "hinter". doc/systems.md, Abschnitt 7.5. */
  if (!reached && idx > 0) {
    const FBWaypoint &from = plan.At(idx - 1);
    double legM = FBPlanarDistM(from.LatDeg, from.LonDeg, wp->LatDeg, wp->LonDeg);
    double course = FBBearingDeg(from.LatDeg, from.LonDeg, wp->LatDeg, wp->LonDeg);
    double alongM = 0.0, acrossM = 0.0;
    FBTrackProjectM(from.LatDeg, from.LonDeg, course, lat, lon, alongM, acrossM);
    if (alongM >= legM) { reached = true; by = "passed"; }
  }
  if (!reached) return -1;
  plan.SetActiveIndex(idx + 1);
  FBLog::Info("nav", "WP_REACHED", {{"idx", idx}, {"lat", wp->LatDeg}, {"lon", wp->LonDeg}, {"by", by}});
  return idx;
}

} // namespace FlightBox::Systems
