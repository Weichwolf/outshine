/* FlightBox — FBMissileGuidance: the missile's PILOT, in exactly the slot systems/FBPilot defines. A
 * jet's pilot picks waypoints; this one runs PROPORTIONAL NAVIGATION. Everything below it is unchanged
 * — the commands travel through Autopilot(Manual)/FlightControl into the SIMULATED FINS, so the
 * anti-cheat rule that governs every pilot governs this one.
 *
 * Under the law sit two lateral-acceleration loops (SKID-TO-TURN: a cruciform round pitches and yaws
 * without banking), each closed on what a missile actually measures — accelerometer error plus a GYRO
 * rate term that is not optional, because the airframe's own pitch damping is nearly nil by design.
 *
 * THREE PHASES, and the handover is an EVENT rather than a timer: the seeker is switched on at the
 * activation range, and the phase changes when it actually ACQUIRES — which it can only do if the
 * midcourse pointed it close enough. A bad midcourse produces a miss, not a magic terminal phase, and
 * losing the uplink drops back to Inertial on the last estimate rather than to a failure path.
 *
 * Full derivation of PN, of N, of the gain schedule and of the phase table:
 * doc/flightbox/weapons-and-damage.md §10.2. */
#ifndef FBMISSILEGUIDANCE_H
#define FBMISSILEGUIDANCE_H

#include "FBPilot.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"

namespace FlightBox {

class FBMissileSeeker;

class FBMissileGuidance : public FBPilot {
public:
  /* Telemetry ordinal — append, never reorder. */
  enum class Phase { Inertial = 0, Midcourse, Terminal };
  static const char *PhaseName(Phase p);

  static constexpr double kNavConstant = 4.0;
  /* Command ceiling [SET]: above what the round can reach except low and fast, so it bounds the ask
   * without ever being the binding limit in a normal engagement. */
  static constexpr double kMaxCommandG = 25.0;
  /* ---- AUTOPILOT GAINS, SCHEDULED ON DYNAMIC PRESSURE. Fin authority is proportional to q, so a fixed
   * gain would be sluggish exactly where the round is slow (right after launch, where the first turn
   * has to be made) and twitchy where it is fast. kFinPerG is the [MESS] inverse of the airframe's
   * measured g-per-fin at kQRefPa; the INTEGRAL is not optional, because a proportional-only loop
   * leaves a steady-state error that the 1 g gravity bias turns into a permanent SINK (measured before
   * the term existed: ~900 m low at the merge). ---- */
  static constexpr double kQRefPa = 119000.0;
  static constexpr double kFinPerG = 1.0 / 13.8;
  static constexpr double kLoopP = 1.2, kLoopI = 2.0;
  static constexpr double kRateGain = 0.35;
  static constexpr double kGainScaleMin = 0.15, kGainScaleMax = 20.0;
  static constexpr double kIntegralClamp = 1.0;
  static constexpr double kRollGain = 0.05, kRollRateGain = 0.02;
  /* [SET] The launcher refreshes at its own radar frame time, so a second without a message means the
   * support has STOPPED, not that one was missed. */
  static constexpr double kUplinkTimeoutS = 1.5;

  /* Wiring, once, by the module; both borrowed. */
  void Bind(const FBStoreSpec &spec, FBMissileSeeker &seeker);
  /* The launch programming: where the shooter's fire control last saw the target, and whose uplink this
   * round listens to. Applied once, at spawn. */
  void Program(const FBWeaponTargetState &target, int launcherId, double launchS);
  int  LauncherId() const { return LauncherId_; }

  /* The same absolute sim time the seeker's grid and the uplink's message ages run on, so nothing in
   * the round has a private notion of now. */
  void SetTime(double simTimeS) { NowS_ = simTimeS; }

  /* The override point: reads its own seeker's and uplink's bus blocks plus the airframe state, returns
   * fin/throttle as a Manual guidance request. `plan`/`runway` unused — a missile has no flight plan. */
  FBPilotCommands Run(const FBState &state, FBCommandBus &avionics, const FBAirframeControls &airframe,
                      const fb_fdm_state &st, const FBFlightPlan &plan, const FBRunway *runway,
                      double dt) override;

  /* A missile's trace is not a jet's, and the bus is per-unit, so this REPLACES the pilot's channels
   * rather than appending: no aircraft's telemetry.csv changes by one column. */
  const char *TelemetryName() const override { return "msl"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  Phase GuidancePhase() const { return Phase_; }

private:
  /* Strict priority: own seeker > uplink > last known; each branch sets Phase_. */
  void UpdateTarget(const FBState &state, const fb_fdm_state &st);
  void EnterPhase(Phase p, const char *why);

  const FBStoreSpec *Spec_ = nullptr;   /* borrowed catalogue entry */
  FBMissileSeeker *Seeker_ = nullptr;   /* borrowed, owned by the module */

  Phase  Phase_ = Phase::Inertial;
  int    LauncherId_ = 0;
  double NowS_ = 0.0, LaunchS_ = 0.0;

  /* The estimate the law flies against, and WHEN it was last actually measured — the age of this stamp
   * is what "flying on the last information" means. */
  bool   HaveTarget_ = false;
  double TgtLatDeg_ = 0.0, TgtLonDeg_ = 0.0, TgtAltM_ = 0.0;
  double TgtVelE_ = 0.0, TgtVelN_ = 0.0, TgtVelU_ = 0.0;
  double TgtStampS_ = -1e9;
  bool   SeekerLocked_ = false;

  double RangeM_ = 0.0, ClosureMs_ = 0.0, LosRateDegS_ = 0.0;
  double NzCmdG_ = 0.0, NyCmdG_ = 0.0;
  double LosAzDeg_ = 0.0, LosElDeg_ = 0.0;
  double FinPitch_ = 0.0, FinYaw_ = 0.0;
  double NzInt_ = 0.0, NyInt_ = 0.0;   /* the two acceleration loops' integrators */
};

} // namespace FlightBox
#endif
