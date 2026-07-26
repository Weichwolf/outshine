#include "FBMissionMonitor.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kMPerDeg = 111320.0;

/* "off the runway" — unchanged geometry from the pre-orchestrator crash gate this replaces (formerly
 * FBMissionRunner.cpp's own OnRunway): project (lat,lon) onto the runway's along/across-track axes
 * (centreline from the threshold on TrueHeadingDeg) — on the runway iff within its length (+
 * marginAlongM before/after) and half-width (+ marginAcrossM either side). WidthM <= 1 (the mission
 * format leaves it unset) falls back to a generous 60 m generic-runway half-width. */
bool OnRunway(const FBRunway &rwy, double lat, double lon, double marginAlongM, double marginAcrossM) {
  double hdg = rwy.TrueHeadingDeg * kPi / 180.0;
  double coslat = std::cos(rwy.ThresholdLatDeg * kPi / 180.0);
  double dy = (lat - rwy.ThresholdLatDeg) * kMPerDeg;
  double dx = (lon - rwy.ThresholdLonDeg) * kMPerDeg * coslat;
  double along = dx * std::sin(hdg) + dy * std::cos(hdg);
  double across = dx * std::cos(hdg) - dy * std::sin(hdg);
  double halfW = (rwy.WidthM > 1.0 ? rwy.WidthM : 60.0) * 0.5 + marginAcrossM;
  return along >= -marginAlongM && along <= rwy.LengthM + marginAlongM && std::fabs(across) <= halfW;
}
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

  /* Waypoint progress against the mission's OWN plan, purely from observed position (class banner). */
  if (ActiveIdx_ >= 0 && ActiveIdx_ < Plan_.Size()) {
    const FBWaypoint &wp = Plan_.At(ActiveIdx_);
    double coslat = std::cos(s.LatDeg * kPi / 180.0);
    double dy = (s.LatDeg - wp.LatDeg) * kMPerDeg, dx = (s.LonDeg - wp.LonDeg) * kMPerDeg * coslat;
    if (std::sqrt(dx * dx + dy * dy) <= WpCaptureM_) {
      FBLog::Info("mission", "WP_REACHED", {{"idx", ActiveIdx_}, {"lat", wp.LatDeg}, {"lon", wp.LonDeg}});
      ActiveIdx_++;
      if (ActiveIdx_ >= Plan_.Size())
        return Conclude(FBMissionVerdict::Success, "all waypoints reached");
    }
  }

  if (simTimeS >= TimeoutS_)
    return Conclude(FBMissionVerdict::Timeout, "sim time exceeded the mission timeout");

  return false;
}

} // namespace FlightBox
