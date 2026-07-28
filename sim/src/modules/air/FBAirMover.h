/* FlightBox — FBAirMover: the KINEMATIC half of the catalogue's two motion laws, and it is deliberately
 * the cheaper half that must not grow.
 *
 * WHY IT EXISTS AT ALL. doc/modules/air/module.md §Spec 4 splits on a two-part test — a row gets a
 * JSBSim deck iff its own manoeuvre decides an outcome AND its envelope is published. Those two are the
 * same fact seen twice: a tanker's, an AWACS's and a bomber's intercept geometry is set by their speed
 * and altitude, both published, and by nothing they do; and nobody ever published a drag polar for a
 * KC-135 because nobody ever needed one. Generating a deck for such a row would be INVENTED
 * AERODYNAMICS WEARING A CITATION.
 *
 * WHAT IT IS: position, altitude, heading, speed. A great-circle leg to the next waypoint at the
 * declared speed, with a turn radius from that speed and a [SET] 25 deg bank so a track has a curve,
 * and a declared climb rate. NOTHING ELSE — no control channel, no trim state, no store carriage
 * aerodynamics, and it is never shown to core/FBFlightMonitor because it has no airframe to judge.
 *
 * THE ONE COLLISION IT HAD TO SOLVE (module.md §Gaps collision 1): a unit without an FBFdm yields
 * `AnyWow = true` by construction, so a KC-135 at 25 000 ft would report weight on wheels to every
 * consumer of that bit. The mover publishes its own airborne state instead — FBModule::Airborne(),
 * default false, so a ground position's `AnyWow = true` stays correct because a launcher really is on
 * the ground. */
#ifndef FBAIRMOVER_H
#define FBAIRMOVER_H

#include "FBAircraft.h"
#include "FBFlightPlan.h"
#include "FBFdm.h"

namespace FlightBox::Modules {

class FBAirMover {
public:
  void Configure(const FBAirMoverSpec &spec) { Spec_ = spec; }

  /* A CLOSED RACETRACK around the last two waypoints. `legM` is the straight-leg length and `periodS`
   * the time for one circuit; absent (legM <= 0) = fly the route once and hold the last leg. It is
   * MISSION data and not catalogue data because a station is a TASKING: W1's tanker track and W3's
   * AWACS orbit are different geometry for the same aircraft. */
  void SetOrbit(double legM, double periodS) { OrbitLegM_ = legM; OrbitPeriodS_ = periodS; }
  bool Orbiting() const { return OrbitLegM_ > 0.0; }

  /* THE T1 REFLEX, commanded by the pilot rather than decided here: turn to a heading and hold it. The
   * mover is a plant, not a decision — which is why the drag reflex lives in FBAirPilot and reads an
   * RWR report, and this class only knows how to fly a heading. */
  void CommandHeading(double deg) { CmdHdgDeg_ = deg; HaveCmdHdg_ = true; }
  void ReleaseHeading() { HaveCmdHdg_ = false; }

  /* One tick of pose integration. `st` is the shared state the owner publishes from, written directly:
   * a mover HAS no other truth, so there is no second copy that could drift. The plan is taken by
   * non-const reference because on a mover the WAYPOINT SEQUENCING is this class's job — there is no
   * pilot phase machine underneath to do it. */
  void Advance(Fdm::fb_fdm_state &st, FBFlightPlan &plan, double dt, double simTimeS);

  double TargetAltM() const { return TargetAltM_; }
  bool Orbited() const { return OrbitEntered_; }

private:
  /* The declared turn radius: R = V^2 / (g * tan(bank)). One formula, one [SET] bank angle, and it is
   * the only shape input a kinematic track has. */
  double TurnRadiusM(double vMs) const;

  FBAirMoverSpec Spec_{};
  double OrbitLegM_ = 0.0, OrbitPeriodS_ = 0.0;
  double CmdHdgDeg_ = 0.0;
  bool   HaveCmdHdg_ = false;
  double TargetAltM_ = 0.0;
  /* The racetrack: an axis, a four-state phase (out, turn, back, turn) and the distance run on the
   * current one. Nothing else — the turn law already limits the rate, so the pattern is commanded as
   * headings and its geometry falls out of the bank angle. */
  double OrbitAxisDeg_ = 0.0, OrbitLegRunM_ = 0.0;
  int    OrbitPhase_ = 0;
  bool   OrbitEntered_ = false;
  bool   Initialised_ = false;
};

} // namespace FlightBox::Modules
#endif
