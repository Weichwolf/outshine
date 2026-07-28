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
#include "FBTelemetry.h"
#include "FBZone.h"

namespace FlightBox {

class FBMissionMonitor;

/* The `zone` telemetry source, a class of its own because FBMissionMonitor already IS one thing to the
 * telemetry bus's caller and a source may carry only one name. It reads the judge's own dwell record —
 * the judge is allowed the truth by construction, and this is that truth being WRITTEN OUT, not handed
 * to anybody who could act on it. doc/air-defence-network.md §9. */
class FBZoneTelemetry : public FBTelemetrySource {
public:
  explicit FBZoneTelemetry(const FBMissionMonitor &owner) : Owner_(owner) {}
  const char *TelemetryName() const override { return "zone"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const FBMissionMonitor &Owner_;
};

enum class FBMissionVerdict { None, Success, Fail, Timeout };
const char *FBMissionVerdictStr(FBMissionVerdict v);

/* One tick's OBSERVED truth — deliberately narrow, nothing module-specific. */
struct FBMissionMonitorSample {
  double LatDeg = 0.0, LonDeg = 0.0;
  double ElevM = 0.0;           /* m ASL — the third dimension a declared zone is a cylinder in */
  bool   AnyWow = false;
  double GroundSpeedKt = 0.0;   /* the stillstand-on-the-runway SUCCESS gate for a Land waypoint */
  /* The shootdown as a MISSION fact: whether the SORTIE is over. Nothing is stopped, frozen or marked
   * dead by it — the unit goes on being integrated until the physics judge has its own say. */
  bool   CombatIneffective = false;
  /* This unit's own release register, the same shape as the health bit above and from the same owner:
   * monotone, true from the first store or burst it ever let go. What `no_fire` is judged against. */
  bool   ReleasedWeapon = false;
  /* The other units as OBSERVED — what a `kill` objective is judged against. A unit is never asked about
   * its opponent, and its opponent never about itself. Empty without combat objectives. */
  FBMissionRoster Roster;
};

class FBMissionMonitor {
public:
  /* Everything is COPIED from the mission file at construction. `wpCaptureM` matches the guidance-side
   * capture radius (FBNavSystem's default) so verdict and flown trajectory agree on "reached". */
  FBMissionMonitor(FBFlightPlan plan, std::vector<FBObjective> objectives, FBRunway runway,
                   bool haveRunway, double timeoutS, std::vector<FBZone> zones = {},
                   double wpCaptureM = 500.0);

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

  /* Null unless the mission declared zones, which is what keeps every existing telemetry.csv identical. */
  FBTelemetrySource *ZoneTelemetry() { return Zones_.empty() ? nullptr : &ZoneSrc_; }

private:
  /* The ONE reader of the declared belt outside this class, and it only writes it to a file. */
  friend class FBZoneTelemetry;
  const std::vector<FBZone> &Zones() const { return Zones_; }
  const std::vector<FBZoneDwell> &ZoneDwell() const { return Dwell2_; }
  /* Cumulative dwell per declared zone, from the observed position alone. Pure bookkeeping, no verdict. */
  void NoteZones(const FBMissionMonitorSample &s, double dtS);

  bool Conclude(FBMissionVerdict v, const std::string &detail);
  /* The approach record to the ACTIVE fix — this class's OWN statement of FBNavSystem's orbit rule,
   * computed from its own plan copy and the observed position alone. doc/systems.md, section 7.5.1. */
  int NoteApproach(double distM);
  /* The cumulative dwell of every `identify`, from the ranges the owner published this tick. Pure
   * bookkeeping, no verdict — and monotone, which is what makes the objective latch. */
  void NoteIdentify(const FBMissionRoster &roster, double dtS);
  /* Survive, Waypoints and Identify are deliberately not decided against the roster — see Finalize,
   * PlanJudged_ and Dwell_. */
  bool ObjectivesMet(const FBMissionMonitorSample &s) const;
  /* The kinds that can only be answered when there is no run left in which to lose them. */
  bool HasDeferredObjective() const;
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
  /* Index-parallel to Objectives_, used by Identify alone: the time spent inside its box and the latch
   * that time sets. Sized once in the constructor, never resized. */
  struct FBIdentifyProgress { double DwellS = 0.0; bool Met = false; };
  std::vector<FBIdentifyProgress> Dwell_;
  /* THE DECLARED BELT, this judge's own copy and nobody else's — index-parallel dwell, monotone, sized
   * once. `avoid zone` is decided against it and the `zone` source writes it out. */
  std::vector<FBZone>      Zones_;
  std::vector<FBZoneDwell> Dwell2_;
  FBZoneTelemetry          ZoneSrc_{*this};
  double       PrevSimT_ = 0.0;     /* the tick length is the caller's, so it is measured, not assumed */

  std::string  PlanDetail_;         /* HOW the plan was completed — the legacy SUCCESS wording */

  FBMissionVerdict Verdict_ = FBMissionVerdict::None;
  std::string      Detail_;
};

} // namespace FlightBox
#endif
