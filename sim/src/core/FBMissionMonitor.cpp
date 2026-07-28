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

/* One failed approach is a departure, two are a lap around a fix the aircraft cannot close.
 * Measured in doc/systems.md, section 7.5.1 — the same number the guidance side arrives at, stated
 * here a second time rather than shared, like the passage rule below. */
constexpr int kOrbitFailures = 2;
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
                                   FBRunway runway, bool haveRunway, double timeoutS,
                                   std::vector<FBZone> zones, double wpCaptureM)
    : Plan_(std::move(plan)), Objectives_(std::move(objectives)), Runway_(runway),
      HaveRunway_(haveRunway), TimeoutS_(timeoutS), WpCaptureM_(wpCaptureM), Zones_(std::move(zones)) {
  /* Without `objective` lines the plan is the WHOLE verdict; with them it is judged only if declared —
   * a BVR mission's `wp` line is a briefed vector, not a place the fighter has to arrive at. */
  PlanJudged_ = Objectives_.empty() || HasObjective(FBObjectiveKind::Waypoints);
  PlanDone_ = !PlanJudged_ || Plan_.Empty();
  Dwell_.resize(Objectives_.size());
  Dwell2_.resize(Zones_.size());
}

void FBZoneTelemetry::DeclareTelemetry(FBTelemetrySchema &schema) const {
  for (const FBZone &z : Owner_.Zones()) {
    schema.Add("zone_" + z.Name + "_in");
    schema.Add("zone_" + z.Name + "_s", "s");
  }
}

void FBZoneTelemetry::SampleTelemetry(FBTelemetryRow &row) const {
  const std::vector<FBZoneDwell> &d = Owner_.ZoneDwell();
  for (size_t i = 0; i < d.size(); i++) { row.Push(d[i].In); row.Push(d[i].DwellS); }
}

/* WHERE IN THE LAYER CAKE, in seconds. The declared cylinder is geometry the judge owns a copy of; the
 * sample is the observed position. No unit is asked and no unit is told. */
void FBMissionMonitor::NoteZones(const FBMissionMonitorSample &s, double dtS) {
  for (size_t i = 0; i < Zones_.size(); i++) {
    bool in = FBZoneContains(Zones_[i], s.LatDeg, s.LonDeg, s.ElevM);
    if (in) Dwell2_[i].DwellS += dtS;
    if (in == Dwell2_[i].In) continue;
    Dwell2_[i].In = in;
    FBLog::Info("zone", in ? "ENTER" : "EXIT", {{"zone", Zones_[i].Name}, {"altM", s.ElevM},
        {"dwellS", Dwell2_[i].DwellS}});
  }
}

bool FBMissionMonitor::HasObjective(FBObjectiveKind kind) const {
  for (const auto &o : Objectives_)
    if (o.Kind == kind) return true;
  return false;
}

bool FBMissionMonitor::HasSurviveObjective() const {
  return HasObjective(FBObjectiveKind::Survive);
}

/* None of these is monotone during the run: an opponent's round can still be in the air, a protected
 * unit can still be hit, and a trigger can still be pulled. Banking any of them early would let a unit
 * keep a SUCCESS it can still lose, so a monitor carrying one stays open until Finalize. */
bool FBMissionMonitor::HasDeferredObjective() const {
  for (const auto &o : Objectives_)
    switch (o.Kind) {
      case FBObjectiveKind::Survive:
      case FBObjectiveKind::Protect:
      case FBObjectiveKind::NoFire:
      case FBObjectiveKind::DenyRelease:
      /* A zone can still be entered while the run lasts, so an exposure budget is never banked early —
       * the same argument `no_fire` makes about a trigger that can still be pulled. */
      case FBObjectiveKind::AvoidZone:
      /* A position that went quiet can still come back on, so an allowance is never banked early — the
       * same argument `no_fire` makes about a trigger that can still be pulled. */
      case FBObjectiveKind::Suppress: return true;
      case FBObjectiveKind::KillUnit:
      case FBObjectiveKind::KillTeam:
      case FBObjectiveKind::Waypoints:
      case FBObjectiveKind::Identify: break;
    }
  return false;
}

/* THE ACCUMULATOR, and it is what makes a NON-MONOTONE roster bit safe to judge on: how long each named
 * unit has RADIATED over the run, never a state. Pure bookkeeping, no verdict. */
void FBMissionMonitor::NoteEmitting(const FBMissionRoster &roster, double dtS) {
  for (size_t i = 0; i < Objectives_.size(); i++) {
    const FBObjective &o = Objectives_[i];
    if (o.Kind != FBObjectiveKind::Suppress) continue;
    for (int k = 0; k < roster.Count; k++) {
      if (!FBObjectiveNames(o, roster.Units[k].Id, roster.Units[k].Team)) continue;
      if (!roster.Units[k].Emitting) continue;
      bool wasMet = Dwell_[i].DwellS <= o.HoldS;
      Dwell_[i].DwellS += dtS;
      if (wasMet && Dwell_[i].DwellS > o.HoldS)
        FBLog::Info("mission", "SUPPRESSION_LOST", {{"unit", roster.Units[k].Id},
            {"emittingS", Dwell_[i].DwellS}, {"allowanceS", o.HoldS}});
    }
  }
}

void FBMissionMonitor::NoteIdentify(const FBMissionRoster &roster, double dtS) {
  for (size_t i = 0; i < Objectives_.size(); i++) {
    const FBObjective &o = Objectives_[i];
    if (o.Kind != FBObjectiveKind::Identify || Dwell_[i].Met) continue;
    for (int k = 0; k < roster.Count; k++) {
      if (!FBObjectiveNames(o, roster.Units[k].Id, roster.Units[k].Team)) continue;
      if (roster.Units[k].RangeM > o.RangeM) break;
      Dwell_[i].DwellS += dtS;
      /* `hold 0` is met by the first tick inside the box; the comparison is against the dwell AFTER
       * this tick, so a hold is never satisfied by standing outside. */
      if (Dwell_[i].DwellS >= o.HoldS) {
        Dwell_[i].Met = true;
        FBLog::Info("mission", "IDENTIFIED", {{"unit", o.TargetId}, {"rangeM", roster.Units[k].RangeM},
                                              {"boxM", o.RangeM}, {"heldS", Dwell_[i].DwellS}});
      }
      break;
    }
  }
}

bool FBMissionMonitor::ObjectivesMet(const FBMissionMonitorSample &s) const {
  for (size_t i = 0; i < Objectives_.size(); i++) {
    const FBObjective &o = Objectives_[i];
    switch (o.Kind) {
      /* Not decided against the roster, and falling through to FBObjectiveMet's "no" would leave them
       * permanently unmet: `survive` is answered in Finalize, `waypoints` by PlanJudged_/PlanDone_. */
      case FBObjectiveKind::Survive:
      case FBObjectiveKind::Waypoints: continue;
      case FBObjectiveKind::Identify:  if (!Dwell_[i].Met) return false; continue;
      case FBObjectiveKind::NoFire:    if (s.ReleasedWeapon) return false; continue;
      case FBObjectiveKind::AvoidZone: {
        double dwell = 0.0;
        for (size_t z = 0; z < Zones_.size(); z++)
          if (Zones_[z].Name == o.TargetId) dwell = Dwell2_[z].DwellS;
        if (dwell > o.HoldS) return false;
        continue;
      }
      /* Judged on the ACCUMULATOR, not on the bit: a position that is quiet right now has not been
       * suppressed if it radiated for a minute first. A unit nobody named is never met rather than
       * vacuously so, exactly as the roster-decidable kinds behave. */
      case FBObjectiveKind::Suppress: {
        bool named = false;
        for (int k = 0; k < s.Roster.Count; k++)
          named = named || FBObjectiveNames(o, s.Roster.Units[k].Id, s.Roster.Units[k].Team);
        if (!named || Dwell_[i].DwellS > o.HoldS) return false;
        continue;
      }
      case FBObjectiveKind::KillUnit:
      case FBObjectiveKind::KillTeam:
      case FBObjectiveKind::Protect:
      case FBObjectiveKind::DenyRelease: if (!FBObjectiveMet(o, s.Roster)) return false; continue;
    }
  }
  return true;
}

int FBMissionMonitor::NoteApproach(double distM) {
  if (ActiveIdx_ != AppIdx_) {
    AppIdx_ = ActiveIdx_; AppClosing_ = true; AppMinM_ = distM; AppMaxM_ = distM; AppFails_ = 0;
  }
  if (AppClosing_) {
    if (distM < AppMinM_) AppMinM_ = distM;
    if (distM > AppMinM_ + WpCaptureM_) { AppFails_++; AppClosing_ = false; AppMaxM_ = distM; }
  } else {
    if (distM > AppMaxM_) AppMaxM_ = distM;
    if (distM < AppMaxM_ - WpCaptureM_) { AppClosing_ = true; AppMinM_ = distM; }
  }
  return AppFails_;
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

  NoteIdentify(s.Roster, simTimeS - PrevSimT_);
  NoteEmitting(s.Roster, simTimeS - PrevSimT_);
  NoteZones(s, simTimeS - PrevSimT_);
  PrevSimT_ = simTimeS;

  /* Shot down — checked FIRST, because everything below assumes an aircraft that could still get there.
   * WHOSE failure it is depends on what the mission declared. */
  if (s.CombatIneffective) {
    if (Objectives_.empty())
      return Conclude(FBMissionVerdict::Fail, "combat ineffective (weapon damage)");
    if (HasSurviveObjective())
      return Conclude(FBMissionVerdict::Fail, "combat ineffective (survive objective lost)");
  }

  /* The two kinds that are lost the instant they are broken. Both read a MONOTONE register of the
   * owner's, so neither can be un-broken later and the conclusion is safe to latch here. */
  for (const auto &o : Objectives_) {
    if (o.Kind == FBObjectiveKind::NoFire && s.ReleasedWeapon)
      return Conclude(FBMissionVerdict::Fail, "weapon released (no_fire objective lost)");
    if (o.Kind != FBObjectiveKind::Protect) continue;
    for (int k = 0; k < s.Roster.Count; k++) {
      const FBUnitObservation &u = s.Roster.Units[k];
      if (u.CombatEffective || !FBObjectiveNames(o, u.Id, u.Team)) continue;
      return Conclude(FBMissionVerdict::Fail, std::string("protected unit lost: ") + u.Id);
    }
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
      double distM = FBPlanarDistM(s.LatDeg, s.LonDeg, wp.LatDeg, wp.LonDeg);
      int fails = NoteApproach(distM);
      bool reached = distM <= WpCaptureM_;
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
      /* ...OR THE AIRCRAFT IS ORBITING IT: a capture circle is a GROUND test of fixed radius, the
       * circle the aircraft can fly lives in the AIR MASS and is carried by the wind. One failed
       * approach is a departure, two are a lap — and only where the plan has somewhere to go, since a
       * fix without a successor is the route's destination and not a step in it. Again a SECOND,
       * independent statement of the guidance-side rule. doc/systems.md, section 7.5.1. */
      if (!reached && fails >= kOrbitFailures && ActiveIdx_ + 1 < Plan_.Size()) {
        reached = true; by = "orbited";
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

  /* SUCCESS needs BOTH halves; the deferred kinds can only be answered once the run is over (Finalize). */
  if (PlanDone_ && !HasDeferredObjective() && ObjectivesMet(s))
    return Conclude(FBMissionVerdict::Success, SuccessDetail(PlanDetail_, !Objectives_.empty()));

  if (simTimeS >= TimeoutS_) return Finalize(s, simTimeS);

  return false;
}

bool FBMissionMonitor::Finalize(const FBMissionMonitorSample &s, double simTimeS) {
  (void)simTimeS;
  if (Concluded()) return false;
  /* `survive` is met exactly here and only here: not combat-ineffective at the end = it survived. */
  if (PlanDone_ && ObjectivesMet(s) &&
      !(HasSurviveObjective() && s.CombatIneffective)) {
    std::string d = SuccessDetail(PlanDetail_, !Objectives_.empty());
    if (HasSurviveObjective()) d += ", survived";
    /* The one kind whose result is worth a line of its own, because the number IS the verdict: how long
     * the position was allowed to radiate and how long it did. */
    for (size_t i = 0; i < Objectives_.size(); i++)
      if (Objectives_[i].Kind == FBObjectiveKind::Suppress)
        FBLog::Info("mission", "SUPPRESSED", {{"target", FBObjectiveStr(Objectives_[i])},
            {"emittingS", Dwell_[i].DwellS}, {"allowanceS", Objectives_[i].HoldS}});
    return Conclude(FBMissionVerdict::Success, d);
  }
  return Conclude(FBMissionVerdict::Timeout, "sim time exceeded the mission timeout");
}

} // namespace FlightBox
