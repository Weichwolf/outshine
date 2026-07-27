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
 * Two instances, two questions, run side by side by the same caller (FBMissionRunner.cpp for gym/
 * native, the WASM frame loop for the browser) — never conflated into one, exactly like
 * core/FBFlightMonitor's own banner describes for itself. */
#ifndef FBMISSIONMONITOR_H
#define FBMISSIONMONITOR_H
#include <string>
#include "FBFlightPlan.h"
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
};

class FBMissionMonitor {
public:
  /* `plan`/`runway`/`haveRunway` are copied from the mission FILE at construction (never the module's
   * own, live-mutated FBFlightPlan) — see the class banner. `wpCaptureM` matches the guidance-side
   * capture radius (FBNavSystem's default) so the mission verdict and the flown trajectory agree on
   * "reached". */
  FBMissionMonitor(FBFlightPlan plan, FBRunway runway, bool haveRunway, double timeoutS,
                   double wpCaptureM = 500.0);

  /* Feeds one tick (simTimeS = cumulative sim seconds). Returns true the ONE tick a verdict is reached
   * — Verdict()/Detail() valid from then on. Latches like FBFlightMonitor::Tick: every later call is a
   * no-op returning false. */
  bool Tick(const FBMissionMonitorSample &s, double simTimeS);

  bool               Concluded() const { return Verdict_ != FBMissionVerdict::None; }
  FBMissionVerdict   Verdict() const { return Verdict_; }
  const std::string &Detail() const { return Detail_; }

private:
  bool Conclude(FBMissionVerdict v, const std::string &detail);

  FBFlightPlan Plan_;
  FBRunway     Runway_;
  bool         HaveRunway_;
  double       TimeoutS_;
  double       WpCaptureM_;
  int          ActiveIdx_ = 0;

  FBMissionVerdict Verdict_ = FBMissionVerdict::None;
  std::string      Detail_;
};

} // namespace FlightBox
#endif
