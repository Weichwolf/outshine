#include "FBPilot.h"
#include <cmath>

namespace FlightBox {

namespace {
const double kDeg2Rad = 3.14159265358979323846 / 180.0;
const double kMPerDeg = 111320.0;      /* metres per degree latitude (spherical approx, matches FBAutopilot) */
const double kMsToKt  = 1.9438444924406;
const double kPreflightHoldS = 2.0;    /* brief preflight pause before the roll (class banner) */

/* Centerline steering law gains (generic — the regulated quantity is cross-track/heading error to the
 * runway axis, not an airframe-specific gain; doc/f16/procedures-takeoff-taxi.md's NWS is the actuator,
 * not this law). Small: the F-16's own steer-cmd-norm -> deg schedule is itself very sensitive at low
 * taxi speed (80 deg/unit at ~6 kt, f16.xml), so a gentle norm command is already a firm correction. */
const double kSteerXtGainPerM   = 0.01;
const double kSteerHdgGainPerDeg = 0.02;
const double kSteerCmdMax = 0.6;

/* Rotation pitch-rate stick: a simple PD to the rotate-attitude target (mirrors FBFlightControl's own
 * Raw-path pitch PD). Full-authority stick (kRotateStickMax=1.0, a real rotation pull, not a gentle
 * in-flight roll-in) — the FLCS's own alpha/g scheduling (f16.xml's elevator-scheduler/alpha-limiter)
 * is what actually keeps AoA safe, same as a real pilot commanding hard aft stick on the roll. */
const double kRotateKp = 0.15, kRotateKd = 0.02, kRotateStickMax = 1.0;
const double kPositiveRateMs = 0.5;      /* liftoff-to-gear-up guard: a real climb, not ground noise */
const double kGearUpAglFt = 10.0;        /* + a firm AGL margin: JSBSim's FGLGear freezes WOW the instant
                                          * gear-pos-norm first drops <=0.99 (no gear-up branch recomputes
                                          * it after that) — starting retraction mid-bounce would freeze a
                                          * stale WOW=true, so wait for a confirmed climb-away first. Kept
                                          * small (not a large altitude buffer): the model accelerates
                                          * through the 5 s gear-transit fast enough in full afterburner
                                          * that starting late risks the 300 kt gear-transit speed limit
                                          * (doc/f16/procedures-takeoff-taxi.md) — matches real technique
                                          * (gear up within a couple of seconds of a positive-rate liftoff,
                                          * not after a long climb-away). */

double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
double Wrap180(double a) { while (a > 180.0) a -= 360.0; while (a < -180.0) a += 360.0; return a; }
} // namespace

const char *FBPilot::PhaseName(Phase p) {
  switch (p) {
    case Phase::Idle: return "Idle";
    case Phase::Preflight: return "Preflight";
    case Phase::Takeoff: return "Takeoff";
    case Phase::Climb: return "Climb";
    case Phase::Route: return "Route";
    case Phase::Approach: return "Approach";
    case Phase::Flare: return "Flare";
    case Phase::Rollout: return "Rollout";
    case Phase::Shutdown: return "Shutdown";
  }
  return "?";
}

FBPilotCommands FBPilot::Run(const FBState &state, const fb_fdm_state &fdm, const FBFlightPlan &plan,
                             const FBRunway *runway, const FBWorld *world, double dt) {
  (void)world;
  PhaseElapsedS += dt;
  FBPilotCommands c{};

  /* Mission waypoint bookkeeping (telemetry cache — class banner): the same active-waypoint distance
   * the mission runner used to compute itself from the outside. */
  ActiveWpCache = plan.ActiveIndex();
  DistToWpCache = -1.0;
  if (const FBWaypoint *awp = plan.ActiveWaypoint()) {
    double dy = (fdm.lat - awp->LatDeg) * kMPerDeg, dx = (fdm.lon - awp->LonDeg) * kMPerDeg * std::cos(fdm.lat * kDeg2Rad);
    DistToWpCache = std::sqrt(dx * dx + dy * dy);
  }

  switch (CurPhase) {
    case Phase::Idle:
      return c;   /* neutral — untouched (see the class banner) */

    case Phase::Preflight: {
      /* A ground-spawn check: not on the ground (e.g. an airborne --fly demo that reaches Preflight
       * without a runway) has nothing sane for a preflight to do — stay neutral (Guidance::None, no
       * airframe demand), matching the pre-Phase-1 contract for every caller that isn't a runway
       * ground-start. */
      if (fb_jsbsim_get_wow(-1) == 0) return c;
      c.Guidance = FBPilotGuidance::Manual;   /* idle throttle, wings level, wheels chocked */
      c.GearDown = true;
      c.WheelBrakeLeft = 1.0; c.WheelBrakeRight = 1.0;
      bool engineOk = fb_jsbsim_engine_running(0) != 0;
      if (engineOk && PhaseElapsedS >= kPreflightHoldS) Transition(Phase::Takeoff);
      return c;
    }

    case Phase::Takeoff: {
      c.Guidance = FBPilotGuidance::Manual;
      c.GearDown = true;
      c.WheelBrakeLeft = 0.0; c.WheelBrakeRight = 0.0;   /* brakes released */
      c.ManualThr = TakeoffThrottleNorm();
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;

      if (runway) {   /* centerline tracking: cross-track + heading error to the runway axis */
        double hdgRad = runway->TrueHeadingDeg * kDeg2Rad;
        double coslat = std::cos(runway->ThresholdLatDeg * kDeg2Rad);
        double dy = (fdm.lat - runway->ThresholdLatDeg) * kMPerDeg;
        double dx = Wrap180(fdm.lon - runway->ThresholdLonDeg) * kMPerDeg * coslat;
        double across = dx * std::cos(hdgRad) - dy * std::sin(hdgRad);   /* + = right of centerline */
        double hdgErr = Wrap180(fdm.yaw - runway->TrueHeadingDeg);       /* + = nose right of the axis */
        c.NosewheelSteer = Clamp(-(kSteerXtGainPerM * across + kSteerHdgGainPerDeg * hdgErr),
                                 -kSteerCmdMax, kSteerCmdMax);
      }

      double vr = RotationSpeedKt(fb_jsbsim_get_weight_lbs());
      double casKt = fdm.cas * kMsToKt;
      if (casKt >= vr - RotationLeadKt()) {
        double target = RotationPitchDeg();
        c.ManualPitch = Clamp(kRotateKp * (target - fdm.pitch) - kRotateKd * fdm.q,
                              -kRotateStickMax, kRotateStickMax);
      } else {
        c.ManualPitch = 0.0;   /* stick neutral until the rotate call */
      }

      if (fb_jsbsim_get_wow(-1) == 0) Transition(Phase::Climb);
      return c;
    }

    case Phase::Climb: {
      const FBWaypoint *wp = plan.ActiveWaypoint();
      if (!wp) { Transition(Phase::Shutdown); return c; }
      c.Guidance = FBPilotGuidance::Direct;
      c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
      c.TargetAltM = wp->AltM;
      c.TargetSpeedKt = ClimbSpeedKt();
      /* Positive rate + a confirmed AGL margin (kGearUpAglFt) + below the gear-transit speed limit ->
       * gear up; otherwise leave GearDown unset (it is still commanded down from Takeoff, the safe
       * default) rather than force a retraction. */
      if (fdm.vy > kPositiveRateMs && state.radarAltFt > kGearUpAglFt && fdm.cas * kMsToKt < GearUpLimitKt())
        c.GearDown = false;
      if (fb_jsbsim_get_gear_pos() <= 0.02) Transition(Phase::Route);
      return c;
    }

    case Phase::Route: {
      const FBWaypoint *wp = plan.ActiveWaypoint();
      if (!wp) { Transition(Phase::Shutdown); return c; }   /* no waypoints left -> the mission SUCCESS gate */
      c.Guidance = FBPilotGuidance::Direct;
      c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
      c.TargetAltM = wp->AltM;
      c.TargetSpeedKt = wp->SpeedKt;
      return c;
    }

    case Phase::Approach:
    case Phase::Flare:
    case Phase::Rollout:
    case Phase::Shutdown:
    default:
      return c;   /* Phase 3 (landing) */
  }
}

void FBPilot::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("phase");
  schema.Add("activeWp");
  schema.Add("distToWpM", "m");
}

void FBPilot::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(std::string(PhaseName(CurPhase)));
  row.Push(ActiveWpCache);
  row.Push(DistToWpCache);
}

} // namespace FlightBox
