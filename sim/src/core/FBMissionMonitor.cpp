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

FBMissionMonitor::FBMissionMonitor(FBFlightPlan plan, std::vector<FBObjective> objectives,
                                   FBRunway runway, bool haveRunway, double timeoutS, double wpCaptureM)
    : Plan_(std::move(plan)), Objectives_(std::move(objectives)), Runway_(runway),
      HaveRunway_(haveRunway), TimeoutS_(timeoutS), WpCaptureM_(wpCaptureM) {
  /* IS THE FLIGHT PLAN PART OF THE VERDICT? Without `objective` lines it is the WHOLE verdict — the
   * format's original judgement, unchanged. With them, the block is the complete statement of what this
   * unit has to achieve, so the plan is only judged if it says so (`objective waypoints`): a BVR
   * mission's `wp` line is a briefed vector for the guidance, not a place the fighter has to arrive at,
   * and reading it as an objective is what would make a decided engagement time out. */
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
    /* Two kinds are not decided against the roster and must be SKIPPED here rather than fall through
     * FBObjectiveMet's "no" — `survive` is answered only at the end of the run (Finalize), and
     * `waypoints` is answered by PlanJudged_/PlanDone_ above, which is the whole reason it exists as a
     * declarable objective. Letting either reach FBObjectiveMet made it permanently unmet, i.e. a unit
     * that declared `kill` AND `waypoints` — the documented way to keep the flight plan judged while
     * also having a combat goal (core/FBObjective.h) — could never succeed. */
    if (o.Kind == FBObjectiveKind::Survive || o.Kind == FBObjectiveKind::Waypoints) continue;
    if (!FBObjectiveMet(o, roster)) return false;
  }
  return true;
}

bool FBMissionMonitor::Conclude(FBMissionVerdict v, const std::string &detail) {
  Verdict_ = v;
  Detail_ = detail;
  /* Self-logs its own conclusion (FBFlightMonitor's "KO" precedent) — every caller (FBMissionRunner's
   * combined final RESULT/SUMMARY, the WASM frame loop with no Runner to synthesize one) sees this the
   * instant it happens, without re-deriving it. */
  FBLog::Info("mission", "RESULT", {{"result", FBMissionVerdictStr(v)}, {"reason", detail}});
  return true;
}

/* The SUCCESS wording. Legacy (no objectives declared) is exactly the plan's own sentence, byte for
 * byte — those strings are in every measured events.log. Objectives append to it, and an actor with
 * objectives and no waypoints has only them to report. */
namespace {
std::string SuccessDetail(const std::string &planDetail, bool haveObjectives) {
  if (!haveObjectives) return planDetail;
  return planDetail.empty() ? "objectives met" : planDetail + ", objectives met";
}
} // namespace

bool FBMissionMonitor::Tick(const FBMissionMonitorSample &s, double simTimeS) {
  if (Concluded()) return false;   /* latched — see the header banner */

  /* Shot down. WHOSE failure that is depends on what the mission declared (class banner): with no
   * objectives it is this unit's own, unchanged; with a `survive` objective it is the declared one;
   * with only a `kill` objective this unit was not told to come home, and its own loss ends nothing.
   * Checked FIRST either way, because everything below it assumes an aircraft that could still get
   * there. */
  if (s.CombatIneffective) {
    if (Objectives_.empty())
      return Conclude(FBMissionVerdict::Fail, "combat ineffective (weapon damage)");
    if (HasSurviveObjective())
      return Conclude(FBMissionVerdict::Fail, "combat ineffective (survive objective lost)");
  }

  /* Mission-level ground-contact judgement: a touchdown the physics-only FBFlightMonitor accepted
   * (survivable) but which happened away from the assigned runway missed the mission's objective. */
  if (HaveRunway_ && s.AnyWow && !OnRunway(Runway_, s.LatDeg, s.LonDeg, 50.0, 30.0))
    return Conclude(FBMissionVerdict::Fail, "touchdown off the assigned runway");

  /* Waypoint progress against the mission's OWN plan, purely from observed position (class banner). A
   * Land waypoint (always the LAST one, doc/mission-format.md's 'land' keyword) does not capture-and-
   * advance like an Enroute one — it needs the aircraft to actually STOP on the runway, not merely fly
   * over the threshold, so SUCCESS there is a standalone condition, never reached via ActiveIdx_ falling
   * off the end of the plan. */
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
      if (FBPlanarDistM(s.LatDeg, s.LonDeg, wp.LatDeg, wp.LonDeg) <= WpCaptureM_) {
        FBLog::Info("mission", "WP_REACHED", {{"idx", ActiveIdx_}, {"lat", wp.LatDeg}, {"lon", wp.LonDeg}});
        ActiveIdx_++;
        if (ActiveIdx_ >= Plan_.Size()) {
          PlanDone_ = true;
          PlanDetail_ = "all waypoints reached";
        }
      }
    }
  }

  /* SUCCESS needs BOTH halves (class banner). A `survive` objective is deliberately NOT one of them
   * here: it can only be answered when the run is over, which is Finalize's job. */
  if (PlanDone_ && !HasSurviveObjective() && KillObjectivesMet(s.Roster))
    return Conclude(FBMissionVerdict::Success, SuccessDetail(PlanDetail_, !Objectives_.empty()));

  if (simTimeS >= TimeoutS_) return Finalize(s, simTimeS);

  return false;
}

bool FBMissionMonitor::Finalize(const FBMissionMonitorSample &s, double simTimeS) {
  (void)simTimeS;
  if (Concluded()) return false;
  /* The end of the run, from this unit's side. `survive` is met exactly here and only here: it is not
   * combat-ineffective, so it is still able to fight, so it survived. Everything else was already
   * decided on the tick it happened. */
  if (PlanDone_ && KillObjectivesMet(s.Roster) &&
      !(HasSurviveObjective() && s.CombatIneffective)) {
    std::string d = SuccessDetail(PlanDetail_, !Objectives_.empty());
    if (HasSurviveObjective()) d += ", survived";
    return Conclude(FBMissionVerdict::Success, d);
  }
  return Conclude(FBMissionVerdict::Timeout, "sim time exceeded the mission timeout");
}

} // namespace FlightBox
