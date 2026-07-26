/* FlightBox — FBMissileGuidance: the missile's PILOT. It sits in exactly the slot systems/FBPilot
 * defines for every module (decide where to go, hand it down as FBPilotCommands once per decision tick)
 * and it overrides the one point that class gives a module whose brain genuinely differs — Run(). A jet's
 * pilot picks waypoints; this one runs a guidance law. Everything below it is unchanged: the commands
 * travel through the module's FBAutopilot (Manual pass-through) and FBFlightControl into
 * FBFdm::SetControls, i.e. through the SIMULATED FINS. Nothing here writes a position, a velocity or an
 * attitude, so the anti-cheat rule that governs every pilot governs this one (CLAUDE.md "Kein Cheaten").
 *
 * =========================== THE GUIDANCE LAW: PROPORTIONAL NAVIGATION ===========================
 * DERIVATION. Let r be the vector from missile to target and v_rel = v_target - v_missile. The line of
 * sight rotates at
 *      Omega = (r x v_rel) / |r|^2                                     [rad/s, a vector along the
 *                                                                       axis the LOS is turning about]
 * and the closing speed is
 *      Vc = -d|r|/dt = -(r . v_rel)/|r|.
 * The KEY GEOMETRIC FACT: if two objects move in straight lines, they collide exactly when the line of
 * sight between them does NOT rotate (Omega = 0) while the range closes. A constant-bearing, decreasing-
 * range contact is on a collision course — the same rule a sailor uses. So the whole task of a guidance
 * law is to drive Omega to zero, and PN does it by commanding acceleration PERPENDICULAR to the LOS,
 * proportional to how fast the LOS is turning and to how fast the range is closing:
 *      a_cmd = N * Vc * (Omega x r_hat).
 * Why the Vc factor: the same LOS rate matters more the less time is left, and Vc/|r| is the inverse of
 * that time — this factor is what makes PN converge instead of chasing.
 *
 * THE NAVIGATION CONSTANT N (kNavConstant). Writing the LOS rate's own dynamics under this law gives
 * lambda_ddot proportional to -(N-2) * lambda_dot / t_go, so:
 *   N <= 2  the LOS rate does not decay at all -> a tail chase, never an intercept;
 *   N  = 3  the classic minimum: the miss distance from a step target manoeuvre decays, and the total
 *           commanded acceleration integral is minimised for a non-manoeuvring target;
 *   N  = 4  the standard for an air-to-air missile against a manoeuvring target: it takes the LOS rate
 *           out faster, so the round arrives with its turn already done rather than pulling hardest at
 *           the end, where it has the least energy left;
 *   N >= 5  amplifies seeker noise and the estimate's own errors into fin activity, which costs energy
 *           in drag long before it buys accuracy.
 * FlightBox uses N = 4, i.e. the standard choice, and it is one constant in one place so a future
 * experiment can measure it rather than argue about it.
 *
 * GRAVITY. PN says nothing about weight: an unbiased law would let the round sag, arrive low and pull up
 * at the end. One g of upward bias is added to the command, which is what a real missile's autopilot
 * does with its own accelerometer.
 *
 * THE AUTOPILOT UNDER THE LAW. The commanded acceleration is resolved into the body frame and flown as
 * two independent lateral-acceleration loops (SKID-TO-TURN — a cruciform missile pitches and yaws
 * without banking, so there is no lift vector to roll), each closed on what a missile actually measures:
 *   fin = Ka * (a_commanded - a_measured_by_accelerometer) - Kd * (body rate from the gyro)
 * The rate term is not optional. The airframe's own aerodynamic pitch damping is nearly nil by design
 * (aim120.xml's Cm_q banner) because that is what a short finned body has; the gyro feedback is what
 * makes the loop stable, exactly as in the real thing. The roll channel holds the fin cross level.
 * ================================================================================================
 *
 * =============================== THE THREE GUIDANCE PHASES ======================================
 * (doc/f16/weapons.md §2.5/§4.4: inertial midcourse + two-way datalink updates from the launch
 * aircraft, active-radar seeker for terminal homing.)
 *   Inertial   — what the round leaves the rail with: the launch programming (core/FBStore.h's
 *                FBStoreRelease::Target), a position and a velocity, extrapolated forward at constant
 *                velocity. Also where the round FALLS BACK to when the launcher stops supporting it.
 *   Midcourse  — a fresh estimate is arriving over the launcher's uplink (modules/missile/
 *                FBMissileUplink); the round re-aims at what the shooter's radar sees now.
 *   Terminal   — the round's OWN seeker has a lock and the guidance flies on its measurements. It stays
 *                terminal from then on: a seeker that has the target does not go back to asking.
 * THE HANDOVER IS AN EVENT, NOT A TIMER. The seeker is switched on when the estimated range falls below
 * the round's activation range (its catalogue figure); the phase changes when the seeker actually
 * ACQUIRES — which it can only do if the midcourse pointed it close enough (the seeker's field of view
 * is +-10 deg). A bad midcourse therefore produces a miss, not a magic terminal phase.
 * LOSING THE UPLINK IS SURVIVABLE AND OBSERVABLE. If the launcher drops its lock the uplink stops; the
 * phase falls back to Inertial and the round keeps flying the last estimate it was given, extrapolated.
 * Whether that is good enough depends on how long ago it was and on what the target did since — which
 * is the entire tactical point, and is measured, not asserted (missions/intercept-lost-lock.fbm). */
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

  /* See the banner's derivation. */
  static constexpr double kNavConstant = 4.0;
  /* Command ceiling [SET]: no fin deflection can buy more than the airframe's trim alpha gives at this
   * altitude, and asking for more only winds the loop up. 25 g is above what the round can reach except
   * low and fast, so it bounds the ask without ever being the binding limit in a normal engagement. */
  static constexpr double kMaxCommandG = 25.0;
  /* ---- AUTOPILOT GAINS, scheduled on dynamic pressure ----
   * The airframe's fin authority is proportional to q: FBTestMissileAirframe measures ~13.8 g per unit
   * of fin command at Mach 2 / 6 km (q = 119 kPa), and the same fin buys a fraction of that at launch
   * speed and a multiple of it low and fast. A FIXED gain would therefore be sluggish exactly where the
   * round is slow (right after launch, where the first turn has to be made) and twitchy where it is
   * fast. So the fin-per-g gain is scaled by qRef/q — the standard missile-autopilot gain schedule, and
   * the reason core/FBAtmosphere.h exists.
   *   kFinPerG   fin command per g of demand at kQRefPa (= 1/13.8, the measured inverse)
   *   kLoopP/I   the loop's proportional and integral factors on top of it. The INTEGRAL is not
   *              optional: a proportional-only acceleration loop leaves a steady-state error of
   *              1/(1+loop gain), and with the 1 g gravity bias that error is a permanent SINK — the
   *              round arrives low, which is exactly what the first flown intercept showed before this
   *              term existed (measured: -18 m/s, ~900 m low at the merge).
   *   kRateGain  fin per rad/s of body rate, scheduled the same way: the fin moment is what damps the
   *              airframe's own nearly-undamped pitch mode (aim120.xml's Cm_q banner). */
  static constexpr double kQRefPa = 119000.0;
  static constexpr double kFinPerG = 1.0 / 13.8;
  static constexpr double kLoopP = 1.2, kLoopI = 2.0;
  static constexpr double kRateGain = 0.35;
  static constexpr double kGainScaleMin = 0.15, kGainScaleMax = 20.0;
  static constexpr double kIntegralClamp = 1.0;
  static constexpr double kRollGain = 0.05, kRollRateGain = 0.02;
  /* How stale an uplink message may be before the round stops calling itself supported [SET]: the
   * launcher's fire control refreshes at its own radar frame time (0.1-1 s), so a second without one
   * means the support has stopped, not that a message was missed. */
  static constexpr double kUplinkTimeoutS = 1.5;

  /* Wiring, once, by the module: the round's own catalogue entry (activation/seeker ranges, arming) and
   * the seeker it points and switches on. Both borrowed. */
  void Bind(const FBStoreSpec &spec, FBMissileSeeker &seeker);
  /* The launch programming (core/FBStore.h's FBStoreRelease): where the shooter's fire control last saw
   * the target, and whose uplink this round listens to. Applied once, at spawn. */
  void Program(const FBWeaponTargetState &target, int launcherId, double launchS);
  int  LauncherId() const { return LauncherId_; }

  /* The module's clock, stamped in before Run — the same absolute sim time the seeker's antenna grid and
   * the uplink's message ages run on, so nothing in the round has a private notion of now. */
  void SetTime(double simTimeS) { NowS_ = simTimeS; }

  /* The override point (systems/FBPilot::Run). Reads the bus (its own seeker's radar block, its own
   * uplink receiver's datalink block) and the airframe state; returns fin/throttle commands as a Manual
   * guidance request. `plan`/`runway` are unused — a missile has no flight plan. */
  FBPilotCommands Run(const FBState &state, FBCommandBus &avionics, const FBAirframeControls &airframe,
                      const fb_fdm_state &st, const FBFlightPlan &plan, const FBRunway *runway,
                      double dt) override;

  /* ---- what the round knows and did, for its own telemetry columns (a missile's trace is not a jet's,
   * and FBTelemetrySource is per-unit, so this replaces the pilot's channels rather than appending to
   * them — no aircraft's telemetry.csv changes by one column) ---- */
  const char *TelemetryName() const override { return "msl"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  Phase GuidancePhase() const { return Phase_; }

private:
  /* Folds this tick's best available target estimate into TgtLat_/TgtVel_ and sets Phase_. */
  void UpdateTarget(const FBState &state, const fb_fdm_state &st);
  void EnterPhase(Phase p, const char *why);

  const FBStoreSpec *Spec_ = nullptr;   /* borrowed catalogue entry */
  FBMissileSeeker *Seeker_ = nullptr;   /* borrowed, owned by the module */

  Phase  Phase_ = Phase::Inertial;
  int    LauncherId_ = 0;
  double NowS_ = 0.0, LaunchS_ = 0.0;

  /* The target estimate the law is flying against, and WHEN it was last actually measured — the age of
   * this stamp is what "flying on the last information" means. */
  bool   HaveTarget_ = false;
  double TgtLatDeg_ = 0.0, TgtLonDeg_ = 0.0, TgtAltM_ = 0.0;
  double TgtVelE_ = 0.0, TgtVelN_ = 0.0, TgtVelU_ = 0.0;
  double TgtStampS_ = -1e9;
  bool   SeekerLocked_ = false;

  /* Telemetry's view of the last tick. */
  double RangeM_ = 0.0, ClosureMs_ = 0.0, LosRateDegS_ = 0.0;
  double NzCmdG_ = 0.0, NyCmdG_ = 0.0;
  double LosAzDeg_ = 0.0, LosElDeg_ = 0.0;
  double FinPitch_ = 0.0, FinYaw_ = 0.0;
  double NzInt_ = 0.0, NyInt_ = 0.0;   /* the two acceleration loops' integrators */
};

} // namespace FlightBox
#endif
