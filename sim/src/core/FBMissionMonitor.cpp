#include "FBMissionMonitor.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
/* "off the runway" — unchanged geometry from the pre-orchestrator crash gate this replaces (formerly
 * FBMissionRunner.cpp's own OnRunway): project (lat,lon) onto the runway's along/across-track axes
 * (centreline from the threshold on TrueHeadingDeg) — on the runway iff within its length (+
 * marginAlongM before/after) and half-width (+ marginAcrossM either side). WidthM <= 1 (the mission
 * format leaves it unset) falls back to a generous 60 m generic-runway half-width. */
bool OnRunway(const FBRunway &rwy, double lat, double lon, double marginAlongM, double marginAcrossM) {
  double along, across;
  FBTrackProjectM(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, rwy.TrueHeadingDeg, lat, lon, along, across);
  double halfW = (rwy.WidthM > 1.0 ? rwy.WidthM : 60.0) * 0.5 + marginAcrossM;
  return along >= -marginAlongM && along <= rwy.LengthM + marginAlongM && std::fabs(across) <= halfW;
}

/* Stillstand-on-the-runway SUCCESS gate (class banner): a taxi-speed threshold, not a full stop — a
 * rolling-to-a-stop aircraft still has a few kt of groundspeed for several ticks, and the mission
 * objective ("landed and stopped") is met well before the literal last knot bleeds off. */
constexpr double kStillstandKt = 2.0;
} // namespace

const char *FBMissionVerdictStr(FBMissionVerdict v) {
  switch (v) {
    case FBMissionVerdict::None: return "NONE";
    case FBMissionVerdict::Success: return "SUCCESS";
    case FBMissionVerdict::Fail: return "FAIL";
    case FBMissionVerdict::Timeout: return "TIMEOUT";
  }
  return "?";
}

FBMissionMonitor::FBMissionMonitor(FBFlightPlan plan, FBRunway runway, bool haveRunway, double timeoutS,
                                   double wpCaptureM)
    : Plan_(std::move(plan)), Runway_(runway), HaveRunway_(haveRunway), TimeoutS_(timeoutS),
      WpCaptureM_(wpCaptureM) {}

bool FBMissionMonitor::Conclude(FBMissionVerdict v, const std::string &detail) {
  Verdict_ = v;
  Detail_ = detail;
  /* Self-logs its own conclusion (FBFlightMonitor's "KO" precedent) — every caller (FBMissionRunner's
   * combined final RESULT/SUMMARY, the WASM frame loop with no Runner to synthesize one) sees this the
   * instant it happens, without re-deriving it. */
  FBLog::Info("mission", "RESULT", {{"result", FBMissionVerdictStr(v)}, {"reason", detail}});
  return true;
}

bool FBMissionMonitor::Tick(const FBMissionMonitorSample &s, double simTimeS) {
  if (Concluded()) return false;   /* latched — see the header banner */

  /* Mission-level ground-contact judgement: a touchdown the physics-only FBFlightMonitor accepted
   * (survivable) but which happened away from the assigned runway missed the mission's objective. */
  if (HaveRunway_ && s.AnyWow && !OnRunway(Runway_, s.LatDeg, s.LonDeg, 50.0, 30.0))
    return Conclude(FBMissionVerdict::Fail, "touchdown off the assigned runway");

  /* Waypoint progress against the mission's OWN plan, purely from observed position (class banner). A
   * Land waypoint (always the LAST one, doc/mission-format.md's 'land' keyword) does not capture-and-
   * advance like an Enroute one — it needs the aircraft to actually STOP on the runway, not merely fly
   * over the threshold, so SUCCESS there is a standalone condition, never reached via ActiveIdx_ falling
   * off the end of the plan. */
  if (ActiveIdx_ >= 0 && ActiveIdx_ < Plan_.Size()) {
    const FBWaypoint &wp = Plan_.At(ActiveIdx_);
    if (wp.Type == FBWaypointType::Land) {
      if (HaveRunway_ && s.AnyWow && s.GroundSpeedKt < kStillstandKt &&
          OnRunway(Runway_, s.LatDeg, s.LonDeg, 0.0, 15.0))
        return Conclude(FBMissionVerdict::Success, "stopped on the runway");
    } else {
      /* Reference = the aircraft (its latitude scales the longitude), the historical convention here. */
      if (FBPlanarDistM(s.LatDeg, s.LonDeg, wp.LatDeg, wp.LonDeg) <= WpCaptureM_) {
        FBLog::Info("mission", "WP_REACHED", {{"idx", ActiveIdx_}, {"lat", wp.LatDeg}, {"lon", wp.LonDeg}});
        ActiveIdx_++;
        if (ActiveIdx_ >= Plan_.Size())
          return Conclude(FBMissionVerdict::Success, "all waypoints reached");
      }
    }
  }

  if (simTimeS >= TimeoutS_)
    return Conclude(FBMissionVerdict::Timeout, "sim time exceeded the mission timeout");

  return false;
}

} // namespace FlightBox
