/* FlightBox — FBPilot: the mission-level brain ABOVE the existing stack (core/ architecture banner) —
 * FBFlightControl (100 Hz, the hands) and FBAutopilot (the manoeuvre executor) stay untouched; FBPilot
 * decides WHERE the aircraft should go and hands that down once per ~10 Hz decision tick as
 * FBPilotCommands: a guidance request for FBAutopilot plus airframe demands for FBAirframeControls. The
 * pilot does not know whose airframe it flies — Ownship today, AI units later, same interface.
 *
 * Phase 1 (the takeoff, doc/f16/procedures-takeoff-taxi.md): Preflight (engine+WOW check, brief hold)
 * -> Takeoff (brakes released, MAX/AB throttle, nosewheel-steering centerline tracking off FBRunway's
 * geometry, stick neutral until Vr then a pitch-rate rotation to the rotate-attitude target, liftoff via
 * WOW==0) -> Climb (FBAutopilot::Direct guidance to the active FBFlightPlan waypoint, gear retracted once
 * a positive rate is established below the gear-up speed limit) -> Route (Direct guidance to the active
 * waypoint at ITS own alt/speed; FBFlightPlan's own WP_REACHED sequencing, driven by the caller, advances
 * the active waypoint — Route just keeps flying whatever is active; the active waypoint turning
 * FBWaypointType::Land hands off to Approach below; no waypoints left -> Shutdown, the mission runner's
 * pre-landing SUCCESS gate).
 *
 * Phase 3 (the landing, doc/f16/procedures-landing.md): Approach (FBAutopilot::Course tracks the
 * assigned runway's extended centerline + a GlidepathAngleDeg descent to the threshold, throttle holds
 * ApproachSpeedKt — the F-16 flies AoA via throttle, not pitch trim, so an on-speed CAS number stands in
 * for a closed AoA loop, see FBF16Pilot's banner for the measured on-speed CAS) -> Flare (below
 * FlareStartAglFt: guidance drops to Manual, throttle to idle, a pitch-rate PD (mirrors Takeoff's rotate
 * law) pulls to FlareTargetPitchDeg to arrest the sink rate for a soft touchdown) -> Rollout (on WOW:
 * hold AerobrakePitchDeg two-point aerodynamic braking — the same pitch-rate PD, target held well under
 * FBFlightMonitor's 15-deg attitude-contact K.O. — until AerobrakeSpeedKt, then derotate the nose down
 * while wheel brakes + the Takeoff phase's own centerline nosewheel-steering law bring it to a stop; the
 * mission's FBMissionMonitor, not this class, judges "stopped on the runway" as SUCCESS).
 *
 * The airframe-specific NUMBERS (rotation speed by weight, rotation pitch, gear-up limit, climb speed,
 * takeoff throttle, approach speed/glidepath, flare/rollout pitch targets and speeds) are the class's
 * virtual hooks below, not hardcoded here — FBF16Pilot supplies the F-16's; the defaults here are a
 * generic placeholder, not a real airframe's numbers.
 *
 * Run() is the one override point: a module whose pilot genuinely differs (not just different
 * procedure numbers — a module with its own decision logic) overrides it, same override-point pattern
 * as FBAutopilot::Run/FBFlightControl::Run; FBF16Pilot (modules/f16/) composes this default and overrides
 * only the numeric hooks. */
#ifndef FBPILOT_H
#define FBPILOT_H

#include <optional>
#include "FBAirframeControls.h"
#include "FBFlightPlan.h"
#include "FBRunway.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

/* The guidance the pilot hands to FBAutopilot. None = "don't touch the AP" — the module only calls the
 * matching FBAutopilot setter (SetManual/SetDirect/SetCourse) when a concrete mode is requested, so a
 * None-guidance tick changes nothing about whatever guidance is already running. */
enum class FBPilotGuidance { None, Manual, Direct, Course };

/* One decision tick's output. Every airframe demand is std::optional: unset = "the pilot isn't
 * touching this control right now" (most ticks touch none of them), exactly mirroring a real pilot's
 * hands — the module only calls the matching FBAirframeControls setter when a field is present. */
struct FBPilotCommands {
  FBPilotGuidance Guidance = FBPilotGuidance::None;

  double TargetAltM = 0.0, TargetSpeedKt = 0.0;
  double TargetLatDeg = 0.0, TargetLonDeg = 0.0;   /* Direct target point / Course reference point */
  double CourseDeg = 0.0, GlidepathDeg = 0.0;      /* Course-only: track heading + descent angle
                                                      (TargetAltM doubles as Course's threshold elevM) */
  double ManualRoll = 0.0, ManualPitch = 0.0, ManualYaw = 0.0, ManualThr = 0.0;   /* Manual pass-through */

  std::optional<bool>   GearDown;
  std::optional<double> Speedbrake;                       /* 0..1 */
  std::optional<double> WheelBrakeLeft, WheelBrakeRight;   /* 0..1 */
  std::optional<double> NosewheelSteer;                    /* -1..1 */
  std::optional<bool>   EngineStart;                       /* true = start commanded, false = cutoff */
};

class FBPilot : public FBTelemetrySource {
public:
  enum class Phase { Idle, Preflight, Takeoff, Climb, Route, Approach, Flare, Rollout, Shutdown };
  static const char *PhaseName(Phase p);

  FBPilot() = default;
  virtual ~FBPilot() = default;

  Phase GetPhase() const { return CurPhase; }
  void  SetPhase(Phase p) { CurPhase = p; PhaseElapsedS = 0.0; }

  /* The one override point (see the class banner). `plan` is the mission's waypoint chain, `runway`
   * the assigned runway for takeoff/landing phases (nullptr while none is assigned). Caches
   * ActiveWpCache/DistToWpCache for SampleTelemetry — the mission-level waypoint bookkeeping this class
   * already needs internally, not a duplicate computation kept only for telemetry.
   *
   * `airframe` is the jet the pilot is flying, borrowed CONST: instrument readings only (WOW, gear
   * position, gross weight, engine running) through the SAME interface the pilot's commands come back
   * out of. The pilot never touches an FDM — it neither knows nor can reach one, which is what keeps
   * this generic layer both airframe-agnostic and instance-agnostic (multi-unit). A module composes its
   * pilot long before any airframe exists, so the handle travels per tick with the rest of the sensed
   * world (`st`, `plan`, `runway`) rather than being bound at construction.
   *
   * NOTHING ELSE IS IN THIS SIGNATURE, and that is the point (CLAUDE.md "Kein Cheaten"): a pilot flies
   * on instruments. It reads FBState — which is what the simulated SENSORS wrote, the datalink's tracks
   * included — plus its own airframe's readings, and has no path to the world or to the unit registry
   * to check what is really out there. The `const FBWorld *` that used to sit here (unused, `(void)
   * world`) was exactly such a path waiting to be taken, so it is gone: a pilot that cannot be handed
   * ground truth cannot accidentally be flown on it. */
  virtual FBPilotCommands Run(const FBState &state, const FBAirframeControls &airframe,
                              const fb_fdm_state &st, const FBFlightPlan &plan, const FBRunway *runway,
                              double dt);

  const char *TelemetryName() const override { return "pilot"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* The airframe's own numbers (class banner) — generic placeholders here, FBF16Pilot overrides every
   * one of them from doc/f16/procedures-takeoff-taxi.md. Not the Run() override point: these are config
   * (like FBFlightControl::F16()'s gain preset), just expressed as virtuals because RotationSpeedKt needs
   * live gross weight, not a boot-time constant. */
  virtual double RotationSpeedKt(double grossWeightLbs) const { (void)grossWeightLbs; return 65.0; }
  virtual double RotationLeadKt() const { return 10.0; }      /* start the rotate pull this far below Vr */
  virtual double RotationPitchDeg() const { return 8.0; }
  virtual double GearUpLimitKt() const { return 150.0; }
  virtual double ClimbSpeedKt() const { return 100.0; }
  virtual double TakeoffThrottleNorm() const { return 1.0; }

  /* Phase 3 (the landing, class banner) — generic placeholders, FBF16Pilot overrides every one from
   * doc/f16/procedures-landing.md (or, where the doc gives no number, a measured on-speed CAS against
   * the vanilla model, see FBF16Pilot's own banner). */
  virtual double ApproachSpeedKt() const { return 90.0; }
  virtual double GlidepathAngleDeg() const { return 3.0; }
  virtual double ApproachSpeedbrakeNorm() const { return 0.5; }
  virtual double FlareStartAglFt() const { return 50.0; }
  virtual double FlareTargetPitchDeg() const { return 8.0; }
  virtual double AerobrakePitchDeg() const { return 10.0; }     /* stays well under FBFlightMonitor's
                                                                   15-deg attitude-contact K.O. (margin) */
  virtual double AerobrakeSpeedKt() const { return 100.0; }     /* nose-down below this */
  virtual double RolloutBrakeNorm() const { return 0.8; }       /* wheel-brake command once derotated */

private:
  void Transition(Phase p) { CurPhase = p; PhaseElapsedS = 0.0; }

  /* Runway-relative along/across-track (m), the SAME axis convention FBMissionMonitor::OnRunway and
   * FBAutopilot::SetCourse use (along=0 at the threshold, +down the runway; +across = right of course) —
   * shared by Takeoff's ground-steering law and Rollout's reuse of it below. */
  static void RunwayAxis(const FBRunway &rwy, double lat, double lon, double &alongM, double &acrossM);
  double NosewheelSteerCmd(const FBRunway &rwy, double lat, double lon, double yawDeg) const;
  double PitchHoldStick(double targetDeg, double pitchDeg, double qDegS, double stickMax) const;

  Phase CurPhase = Phase::Idle;
  double PhaseElapsedS = 0.0;
  int ActiveWpCache = -1;         /* telemetry cache, set in Run() (class banner) */
  double DistToWpCache = -1.0;
};

} // namespace FlightBox
#endif
