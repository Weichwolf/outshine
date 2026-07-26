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

/* Pitch-attitude-hold stick: a simple PD to a target pitch (mirrors flightctl.h's own Raw-path pitch
 * PD) — Takeoff's rotation pull, Flare's touchdown flare, and Rollout's aerobrake/derotate all reduce to
 * "hold this pitch attitude", just with a different target/stick-authority cap per phase (kRotateStickMax
 * below is Takeoff/Rollout's full-authority pull; Flare uses a gentler cap, see kFlareStickMax). */
const double kRotateKp = 0.15, kRotateKd = 0.02, kRotateStickMax = 1.0;
const double kFlareStickMax = 0.6;      /* gentler near the ground — the flare is a soft correction, not
                                          * a rotation pull */
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

/* along=0 at the threshold, +down the runway heading; +across = right of the runway axis — the SAME
 * convention FBMissionMonitor::OnRunway and FBAutopilot::SetCourse use (class banner), so Takeoff's
 * ground steering, Rollout's reuse of it, and the mission verdict all agree on "on the line". */
void FBPilot::RunwayAxis(const FBRunway &rwy, double lat, double lon, double &alongM, double &acrossM) {
  double hdgRad = rwy.TrueHeadingDeg * kDeg2Rad;
  double coslat = std::cos(rwy.ThresholdLatDeg * kDeg2Rad);
  double dy = (lat - rwy.ThresholdLatDeg) * kMPerDeg;
  double dx = Wrap180(lon - rwy.ThresholdLonDeg) * kMPerDeg * coslat;
  alongM = dx * std::sin(hdgRad) + dy * std::cos(hdgRad);
  acrossM = dx * std::cos(hdgRad) - dy * std::sin(hdgRad);
}

double FBPilot::NosewheelSteerCmd(const FBRunway &rwy, double lat, double lon, double yawDeg) const {
  double along, across;
  RunwayAxis(rwy, lat, lon, along, across);
  (void)along;
  double hdgErr = Wrap180(yawDeg - rwy.TrueHeadingDeg);
  return Clamp(-(kSteerXtGainPerM * across + kSteerHdgGainPerDeg * hdgErr), -kSteerCmdMax, kSteerCmdMax);
}

double FBPilot::PitchHoldStick(double targetDeg, double pitchDeg, double qDegS, double stickMax) const {
  return Clamp(kRotateKp * (targetDeg - pitchDeg) - kRotateKd * qDegS, -stickMax, stickMax);
}

FBPilotCommands FBPilot::Run(const FBState &state, const FBAirframeControls &airframe,
                             const fb_fdm_state &st, const FBFlightPlan &plan, const FBRunway *runway,
                             const FBWorld *world, double dt) {
  (void)world;
  PhaseElapsedS += dt;
  FBPilotCommands c{};

  /* Mission waypoint bookkeeping (telemetry cache — class banner): the same active-waypoint distance
   * the mission runner used to compute itself from the outside. */
  ActiveWpCache = plan.ActiveIndex();
  DistToWpCache = -1.0;
  if (const FBWaypoint *awp = plan.ActiveWaypoint()) {
    double dy = (st.lat - awp->LatDeg) * kMPerDeg, dx = (st.lon - awp->LonDeg) * kMPerDeg * std::cos(st.lat * kDeg2Rad);
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
      if (!airframe.GetWeightOnWheels()) return c;
      c.Guidance = FBPilotGuidance::Manual;   /* idle throttle, wings level, wheels chocked */
      c.GearDown = true;
      c.WheelBrakeLeft = 1.0; c.WheelBrakeRight = 1.0;
      bool engineOk = airframe.GetEngineRunning(0);
      if (engineOk && PhaseElapsedS >= kPreflightHoldS) Transition(Phase::Takeoff);
      return c;
    }

    case Phase::Takeoff: {
      c.Guidance = FBPilotGuidance::Manual;
      c.GearDown = true;
      c.WheelBrakeLeft = 0.0; c.WheelBrakeRight = 0.0;   /* brakes released */
      c.ManualThr = TakeoffThrottleNorm();
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;

      if (runway) c.NosewheelSteer = NosewheelSteerCmd(*runway, st.lat, st.lon, st.yaw);

      double vr = RotationSpeedKt(airframe.GetGrossWeightLbs());
      double casKt = st.cas * kMsToKt;
      if (casKt >= vr - RotationLeadKt())
        c.ManualPitch = PitchHoldStick(RotationPitchDeg(), st.pitch, st.q, kRotateStickMax);
      else
        c.ManualPitch = 0.0;   /* stick neutral until the rotate call */

      if (!airframe.GetWeightOnWheels()) Transition(Phase::Climb);
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
      if (st.vy > kPositiveRateMs && state.radarAltFt > kGearUpAglFt && st.cas * kMsToKt < GearUpLimitKt())
        c.GearDown = false;
      if (airframe.GetGearPosition() <= 0.02) Transition(Phase::Route);
      return c;
    }

    case Phase::Route: {
      const FBWaypoint *wp = plan.ActiveWaypoint();
      if (!wp) { Transition(Phase::Shutdown); return c; }   /* no waypoints left -> the mission SUCCESS gate */
      if (wp->Type == FBWaypointType::Land) { Transition(Phase::Approach); return c; }
      c.Guidance = FBPilotGuidance::Direct;
      c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
      c.TargetAltM = wp->AltM;
      c.TargetSpeedKt = wp->SpeedKt;
      return c;
    }

    case Phase::Approach: {
      /* No assigned runway -> nothing sane to land on; stay neutral rather than guess (mirrors
       * Preflight's own "nothing to do" contract). */
      if (!runway) { Transition(Phase::Shutdown); return c; }
      /* Touched down before FlareStartAglFt tripped (a short/high-sink final) — Rollout still handles
       * it correctly (aerobrake-or-derotate is purely a CAS schedule, not conditioned on having flared). */
      if (airframe.GetWeightOnWheels()) { Transition(Phase::Rollout); return c; }

      c.Guidance = FBPilotGuidance::Course;
      c.TargetLatDeg = runway->ThresholdLatDeg; c.TargetLonDeg = runway->ThresholdLonDeg;
      c.CourseDeg = runway->TrueHeadingDeg;
      c.TargetAltM = runway->ThresholdElevM;
      c.GlidepathDeg = GlidepathAngleDeg();
      c.TargetSpeedKt = ApproachSpeedKt();
      c.Speedbrake = ApproachSpeedbrakeNorm();
      if (st.cas * kMsToKt < GearUpLimitKt()) c.GearDown = true;

      if (state.radarAltFt <= FlareStartAglFt()) Transition(Phase::Flare);
      return c;
    }

    case Phase::Flare: {
      if (airframe.GetWeightOnWheels()) { Transition(Phase::Rollout); return c; }
      c.Guidance = FBPilotGuidance::Manual;
      c.ManualThr = 0.0;      /* throttle to idle (doc/f16/procedures-landing.md's Short Final) */
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;
      c.ManualPitch = PitchHoldStick(FlareTargetPitchDeg(), st.pitch, st.q, kFlareStickMax);
      c.Speedbrake = ApproachSpeedbrakeNorm();
      return c;
    }

    case Phase::Rollout: {
      c.Guidance = FBPilotGuidance::Manual;
      c.ManualThr = 0.0;
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;
      c.Speedbrake = 1.0;   /* fully open (doc/f16/procedures-landing.md's Roll-Out) */

      /* Two-point aerodynamic braking above AerobrakeSpeedKt (target held at AerobrakePitchDeg, well
       * under FBFlightMonitor's 15-deg attitude-contact K.O.), a proportional derotate to nose-wheel-down
       * below it — same PD as Takeoff's rotate/Flare's flare, just a speed-scheduled target. */
      double casKt = st.cas * kMsToKt;
      double brakeKt = AerobrakeSpeedKt();
      double targetPitch = casKt > brakeKt ? AerobrakePitchDeg()
                                            : AerobrakePitchDeg() * Clamp(casKt / brakeKt, 0.0, 1.0);
      c.ManualPitch = PitchHoldStick(targetPitch, st.pitch, st.q, kRotateStickMax);

      if (runway) c.NosewheelSteer = NosewheelSteerCmd(*runway, st.lat, st.lon, st.yaw);

      /* Wheel brakes: negligible while the nose is still held up (two-point stance, doc: "F-16 wheel
       * brakes are weak" — aerobrake does the work), moderate-heavy once derotating toward the nosewheel. */
      double brake = casKt <= brakeKt ? RolloutBrakeNorm() : 0.0;
      c.WheelBrakeLeft = brake; c.WheelBrakeRight = brake;
      return c;
    }

    case Phase::Shutdown:
    default:
      return c;
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
