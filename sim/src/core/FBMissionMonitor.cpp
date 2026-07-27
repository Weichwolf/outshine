#include "FBMissionMonitor.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox {

namespace {
/* Project onto the runway's along/across axes: on it iff inside length + margin and half-width +
 * margin. WidthM <= 1 (the mission format leaves it unset) falls back to a generic 60 m width. */
bool OnRunway(const FBRunway &rwy, double lat, double lon, double marginAlongM, double marginAcrossM) {
  double along, across;
  FBTrackProjectM(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, rwy.TrueHeadingDeg, lat, lon, along, across);
  double halfW = (rwy.WidthM > 1.0 ? rwy.WidthM : 60.0) * 0.5 + marginAcrossM;
  return along >= -marginAlongM && along <= rwy.LengthM + marginAlongM && std::fabs(across) <= halfW;
}

/* A taxi-speed threshold, not a full stop: "landed and stopped" is met before the last knot bleeds. */
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

FBMissionMonitor::FBMissionMonitor(FBFlightPlan plan, std::vector<FBObjective> objectives,
                                   FBRunway runway, bool haveRunway, double timeoutS, double wpCaptureM)
    : Plan_(std::move(plan)), Objectives_(std::move(objectives)), Runway_(runway),
      HaveRunway_(haveRunway), TimeoutS_(timeoutS), WpCaptureM_(wpCaptureM) {
  /* Without `objective` lines the plan is the WHOLE verdict; with them it is judged only if declared —
   * a BVR mission's `wp` line is a briefed vector, not a place the fighter has to arrive at. */
  PlanJudged_ = Objectives_.empty() || HasObjective(FBObjectiveKind::Waypoints);
  PlanDone_ = !PlanJudged_ || Plan_.Empty();
}

bool FBMissionMonitor::HasObjective(FBObjectiveKind kind) const {
  for (const auto &o : Objectives_)
    if (o.Kind == kind) return true;
  return false;
}

bool FBMissionMonitor::HasSurviveObjective() const {
  return HasObjective(FBObjectiveKind::Survive);
}

bool FBMissionMonitor::KillObjectivesMet(const FBMissionRoster &roster) const {
  for (const auto &o : Objectives_) {
    /* Neither is decided against the roster, and falling through to FBObjectiveMet's "no" would leave
     * them permanently unmet: `survive` is answered in Finalize, `waypoints` by PlanJudged_/PlanDone_. */
    if (o.Kind == FBObjectiveKind::Survive || o.Kind == FBObjectiveKind::Waypoints) continue;
    if (!FBObjectiveMet(o, roster)) return false;
  }
  return true;
}

bool FBMissionMonitor::Conclude(FBMissionVerdict v, const std::string &detail) {
  Verdict_ = v;
  Detail_ = detail;
  /* Self-logs, so every caller sees the conclusion the instant it happens without re-deriving it. */
  FBLog::Info("mission", "RESULT", {{"result", FBMissionVerdictStr(v)}, {"reason", detail}});
  return true;
}

/* Without objectives the wording is exactly the plan's own sentence, byte for byte — those strings are
 * in every measured events.log. */
namespace {
std::string SuccessDetail(const std::string &planDetail, bool haveObjectives) {
  if (!haveObjectives) return planDetail;
  return planDetail.empty() ? "objectives met" : planDetail + ", objectives met";
}
} // namespace

bool FBMissionMonitor::Tick(const FBMissionMonitorSample &s, double simTimeS) {
  if (Concluded()) return false;   /* latched */

  /* Shot down — checked FIRST, because everything below assumes an aircraft that could still get there.
   * WHOSE failure it is depends on what the mission declared. */
  if (s.CombatIneffective) {
    if (Objectives_.empty())
      return Conclude(FBMissionVerdict::Fail, "combat ineffective (weapon damage)");
    if (HasSurviveObjective())
      return Conclude(FBMissionVerdict::Fail, "combat ineffective (survive objective lost)");
  }

  /* A touchdown the physics judge accepted as survivable, but in the wrong place. */
  if (HaveRunway_ && s.AnyWow && !OnRunway(Runway_, s.LatDeg, s.LonDeg, 50.0, 30.0))
    return Conclude(FBMissionVerdict::Fail, "touchdown off the assigned runway");

  /* A Land waypoint (always the LAST) does not capture-and-advance: it needs the aircraft to STOP on
   * the runway, so SUCCESS there is standalone, never ActiveIdx_ falling off the end of the plan. */
  if (PlanJudged_ && ActiveIdx_ >= 0 && ActiveIdx_ < Plan_.Size()) {
    const FBWaypoint &wp = Plan_.At(ActiveIdx_);
    if (wp.Type == FBWaypointType::Land) {
      if (HaveRunway_ && s.AnyWow && s.GroundSpeedKt < kStillstandKt &&
          OnRunway(Runway_, s.LatDeg, s.LonDeg, 0.0, 15.0)) {
        PlanDone_ = true;
        PlanDetail_ = "stopped on the runway";
      }
    } else {
      /* Reference = the aircraft (its latitude scales the longitude), the historical convention here. */
      bool reached = FBPlanarDistM(s.LatDeg, s.LonDeg, wp.LatDeg, wp.LonDeg) <= WpCaptureM_;
      const char *by = "capture";
      /* ...OR THE AIRCRAFT IS PAST IT: for a fix inside its own turn radius, "did it get there" is
       * answered by the leg's axis. From the second waypoint on, since only then is there an inbound
       * track. Deliberately a SECOND, independent statement of FBNavSystem's rule, not a call into it —
       * this class judges from its own copy alone. */
      if (!reached && ActiveIdx_ > 0) {
        const FBWaypoint &from = Plan_.At(ActiveIdx_ - 1);
        double legM = FBPlanarDistM(from.LatDeg, from.LonDeg, wp.LatDeg, wp.LonDeg);
        double course = FBBearingDeg(from.LatDeg, from.LonDeg, wp.LatDeg, wp.LonDeg);
        double alongM = 0.0, acrossM = 0.0;
        FBTrackProjectM(from.LatDeg, from.LonDeg, course, s.LatDeg, s.LonDeg, alongM, acrossM);
        if (alongM >= legM) { reached = true; by = "passed"; }
      }
      if (reached) {
        FBLog::Info("mission", "WP_REACHED",
                    {{"idx", ActiveIdx_}, {"lat", wp.LatDeg}, {"lon", wp.LonDeg}, {"by", by}});
        ActiveIdx_++;
        if (ActiveIdx_ >= Plan_.Size()) {
          PlanDone_ = true;
          PlanDetail_ = "all waypoints reached";
        }
      }
    }
  }

  /* SUCCESS needs BOTH halves; `survive` can only be answered once the run is over (Finalize). */
  if (PlanDone_ && !HasSurviveObjective() && KillObjectivesMet(s.Roster))
    return Conclude(FBMissionVerdict::Success, SuccessDetail(PlanDetail_, !Objectives_.empty()));

  if (simTimeS >= TimeoutS_) return Finalize(s, simTimeS);

  return false;
}

bool FBMissionMonitor::Finalize(const FBMissionMonitorSample &s, double simTimeS) {
  (void)simTimeS;
  if (Concluded()) return false;
  /* `survive` is met exactly here and only here: not combat-ineffective at the end = it survived. */
  if (PlanDone_ && KillObjectivesMet(s.Roster) &&
      !(HasSurviveObjective() && s.CombatIneffective)) {
    std::string d = SuccessDetail(PlanDetail_, !Objectives_.empty());
    if (HasSurviveObjective()) d += ", survived";
    return Conclude(FBMissionVerdict::Success, d);
  }
  return Conclude(FBMissionVerdict::Timeout, "sim time exceeded the mission timeout");
}

} // namespace FlightBox
