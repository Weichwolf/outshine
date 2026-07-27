/* FlightBox — FBMissionMonitor: the ONE mission-level verdict authority — FBFlightMonitor's sibling
 * (core/, same incorruptible structure: Runner-/App-owned, fed a read-only per-tick sample, a module
 * never sees or holds a reference to an instance), but a DIFFERENT question. FBFlightMonitor asks "did
 * the airframe survive" (physics only, no runway/mission knowledge, untouched by this class). This one
 * asks "did the MISSION succeed" — waypoints reached, ground contact outside the assigned runway,
 * timeout, and, when the mission's own FBFlightPlan ends in a `land` waypoint (FBWaypointType::Land,
 * always the runway threshold, doc/mission-format.md), SUCCESS is redefined from "reached that point" to
 * "came to a stop ON the runway" (weight-on-wheels, groundspeed under kStillstandKt, position inside the
 * runway footprint) — a mission that only clips the threshold at flying speed has not landed. Sourced
 * from the MISSION FILE, not from whatever a module/pilot claims about itself: waypoint progress is
 * tracked against a PRIVATE COPY of the mission's own FBFlightPlan taken at construction (never the
 * module's live, module-mutated
 * FBFlightPlan — FBNavSystem's own waypoint-advance drives GUIDANCE, this one drives the VERDICT) purely
 * from the aircraft's observed position, so a module cannot self-report its way to SUCCESS.
 *
 * COMBAT OBJECTIVES (core/FBObjective.h) are the second half of that question and are judged the same
 * way: from a private copy of what the mission FILE declared, against an observed roster of the other
 * units (id, faction, and the one bit their own health registers publish). Three rules, and all three
 * exist so that a mission that declares nothing behaves exactly as it did before objectives existed:
 *   - NO objectives declared: being shot down is this unit's own FAIL, unchanged. That is the legacy
 *     reading and it stays the default, because a mission that never said what the shot was FOR has
 *     nothing else to read into it.
 *   - `survive` declared: being shot down is a FAIL for the declared reason. Its converse cannot be
 *     latched early — "still able to fight" is only true when there is no run left in which to be shot
 *     — so a unit carrying one is left unconcluded until Finalize (below). A missile already in the air
 *     when its shooter died is exactly why there is no shortcut here.
 *   - `kill` declared and NOT `survive`: this unit's own loss is not a declared failure. A simultaneous
 *     exchange is then a trade rather than a failure of both sides, which is a mission-design choice
 *     the file makes, not one this class makes for it.
 * SUCCESS needs BOTH halves: every waypoint reached (the plan, unchanged) and every kill objective met.
 *
 * Two instances, two questions, run side by side by the same caller (FBMissionRunner.cpp for gym/
 * native, the WASM frame loop for the browser) — never conflated into one, exactly like
 * core/FBFlightMonitor's own banner describes for itself. */
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

/* One tick's worth of OBSERVED truth — deliberately narrow (see FBFlightMonitor.h's own banner for the
 * same pattern): position + whether any gear is currently weight-bearing, nothing module-specific. */
struct FBMissionMonitorSample {
  double LatDeg = 0.0, LonDeg = 0.0;
  bool   AnyWow = false;
  double GroundSpeedKt = 0.0;   /* the stillstand-on-the-runway SUCCESS gate (Land waypoint, class banner) */
  /* THE SHOOTDOWN, as a MISSION fact (core/FBSystemHealth::CombatEffective — engine, flight controls or
   * structure gone). It belongs here and not in FBFlightMonitor for the reason that separates the two
   * judges at all: the physics judge asks whether the airframe survived, and a jet whose engine has just
   * been shot out is still flying and has survived nothing yet. Whether its SORTIE is over is a mission
   * question, and this is the mission judge. The unit is not stopped, frozen or marked dead by it — it
   * goes on being integrated until the physics judge has its own say. */
  bool   CombatIneffective = false;
  /* THE OTHER UNITS, as observed (core/FBObjective.h): id, faction, and the one bit their own health
   * register publishes. It is what a `kill` objective is judged against, and it is filled by the same
   * client that owns those registers — a unit is never asked about its opponent, and its opponent is
   * never asked about itself. Empty for a mission that declares no combat objectives. */
  FBMissionRoster Roster;
};

class FBMissionMonitor {
public:
  /* `plan`/`runway`/`haveRunway` are copied from the mission FILE at construction (never the module's
   * own, live-mutated FBFlightPlan) — see the class banner. `wpCaptureM` matches the guidance-side
   * capture radius (FBNavSystem's default) so the mission verdict and the flown trajectory agree on
   * "reached". */
  FBMissionMonitor(FBFlightPlan plan, std::vector<FBObjective> objectives, FBRunway runway,
                   bool haveRunway, double timeoutS, double wpCaptureM = 500.0);

  /* Feeds one tick (simTimeS = cumulative sim seconds). Returns true the ONE tick a verdict is reached
   * — Verdict()/Detail() valid from then on. Latches like FBFlightMonitor::Tick: every later call is a
   * no-op returning false. */
  bool Tick(const FBMissionMonitorSample &s, double simTimeS);

  /* THE END OF THE RUN, when it is not this unit's own clock that ended it. A `survive` objective is
   * the one objective that cannot be met early — "still able to fight" is only true once there is no
   * more run in which to be shot — so a unit carrying one is deliberately left unconcluded while the
   * engagement is going on, and asked here. Idempotent and latching like Tick; a no-op for a monitor
   * that already concluded, which is every legacy one by the time the loop leaves (their timeout
   * branch concludes them from inside Tick). */
  bool Finalize(const FBMissionMonitorSample &s, double simTimeS);

  /* What this unit was told to achieve — the mission file's own list, copied at construction. Read by
   * the client's combination rule to ask whether another unit's loss was somebody's declared goal
   * (app/FBMissionRunner.cpp); never written, and never shown to a module. */
  const std::vector<FBObjective> &Objectives() const { return Objectives_; }

  bool               Concluded() const { return Verdict_ != FBMissionVerdict::None; }
  FBMissionVerdict   Verdict() const { return Verdict_; }
  const std::string &Detail() const { return Detail_; }

private:
  bool Conclude(FBMissionVerdict v, const std::string &detail);
  /* Every declared KILL objective met? (Survive is never "met" before the end — see Finalize.) */
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
  bool         PlanJudged_ = true;  /* is the flight plan part of the verdict? (see the constructor) */
  bool         PlanDone_ = false;   /* every waypoint captured (trivially true if it is not judged) */

  std::string  PlanDetail_;         /* HOW the plan was completed — the legacy SUCCESS wording */

  FBMissionVerdict Verdict_ = FBMissionVerdict::None;
  std::string      Detail_;
};

} // namespace FlightBox
#endif
