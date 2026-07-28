/* The ONE mission-level verdict authority — FBFlightMonitor's sibling, same incorruptible structure but
 * a DIFFERENT question: "did the MISSION succeed" (waypoints, off-runway touchdown, timeout, combat
 * objectives). Judged from a PRIVATE COPY of the mission FILE and observed position/roster alone, so no
 * module can self-report its way to SUCCESS. Check order, the three objective rules and the capture
 * geometry: doc/core.md, Abschnitt 4.2. */
#ifndef FBMISSIONMONITOR_H
#define FBMISSIONMONITOR_H
#include <string>
#include <vector>
#include "FBFlightPlan.h"
#include "FBObjective.h"
#include "FBRunway.h"

namespace FlightBox {

enum class FBMissionVerdict { None, Success, Fail, Timeout };
const char *FBMissionVerdictStr(FBMissionVerdict v);

/* One tick's OBSERVED truth — deliberately narrow, nothing module-specific. */
struct FBMissionMonitorSample {
  double LatDeg = 0.0, LonDeg = 0.0;
  bool   AnyWow = false;
  double GroundSpeedKt = 0.0;   /* the stillstand-on-the-runway SUCCESS gate for a Land waypoint */
  /* The shootdown as a MISSION fact: whether the SORTIE is over. Nothing is stopped, frozen or marked
   * dead by it — the unit goes on being integrated until the physics judge has its own say. */
  bool   CombatIneffective = false;
  /* The other units as OBSERVED — what a `kill` objective is judged against. A unit is never asked about
   * its opponent, and its opponent never about itself. Empty without combat objectives. */
  FBMissionRoster Roster;
};

class FBMissionMonitor {
public:
  /* Everything is COPIED from the mission file at construction. `wpCaptureM` matches the guidance-side
   * capture radius (FBNavSystem's default) so verdict and flown trajectory agree on "reached". */
  FBMissionMonitor(FBFlightPlan plan, std::vector<FBObjective> objectives, FBRunway runway,
                   bool haveRunway, double timeoutS, double wpCaptureM = 500.0);

  /* Returns true the ONE tick a verdict is reached, then latches like FBFlightMonitor::Tick. */
  bool Tick(const FBMissionMonitorSample &s, double simTimeS);

  /* The end of the run, when it was not this unit's own clock that ended it. `survive` is the one
   * objective that cannot be met early, so a unit carrying one stays unconcluded until asked here. */
  bool Finalize(const FBMissionMonitorSample &s, double simTimeS);

  /* Read by the client's combination rule to ask whether another unit's loss was a declared goal;
   * never written, never shown to a module. */
  const std::vector<FBObjective> &Objectives() const { return Objectives_; }

  bool               Concluded() const { return Verdict_ != FBMissionVerdict::None; }
  FBMissionVerdict   Verdict() const { return Verdict_; }
  const std::string &Detail() const { return Detail_; }

private:
  bool Conclude(FBMissionVerdict v, const std::string &detail);
  /* The approach record to the ACTIVE fix — this class's OWN statement of FBNavSystem's orbit rule,
   * computed from its own plan copy and the observed position alone. doc/systems.md, section 7.5.1. */
  int NoteApproach(double distM);
  /* Survive and Waypoints are deliberately skipped here — see Finalize resp. PlanJudged_. */
  bool KillObjectivesMet(const FBMissionRoster &roster) const;
  bool HasObjective(FBObjectiveKind kind) const;
  bool HasSurviveObjective() const;

  FBFlightPlan Plan_;
  std::vector<FBObjective> Objectives_;
  FBRunway     Runway_;
  bool         HaveRunway_;
  double       TimeoutS_;
  double       WpCaptureM_;
  int          ActiveIdx_ = 0;
  int          AppIdx_ = -1;
  bool         AppClosing_ = true;
  double       AppMinM_ = 0.0, AppMaxM_ = 0.0;
  int          AppFails_ = 0;
  bool         PlanJudged_ = true;  /* is the flight plan part of the verdict? (set in the constructor) */
  bool         PlanDone_ = false;   /* every waypoint captured (trivially true if it is not judged) */

  std::string  PlanDetail_;         /* HOW the plan was completed — the legacy SUCCESS wording */

  FBMissionVerdict Verdict_ = FBMissionVerdict::None;
  std::string      Detail_;
};

} // namespace FlightBox
#endif
