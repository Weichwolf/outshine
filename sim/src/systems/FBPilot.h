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
 * the active waypoint — Route just keeps flying whatever is active; no waypoints left -> Shutdown, the
 * mission runner's SUCCESS gate). Approach/Flare/Rollout stay the Round-A neutral placeholder (Phase 3).
 * The airframe-specific NUMBERS (rotation speed by weight, rotation pitch, gear-up limit, climb speed,
 * takeoff throttle) are the class's virtual hooks below, not hardcoded here — FBF16Pilot supplies the
 * F-16's (doc/f16/procedures-takeoff-taxi.md); the defaults here are a generic placeholder, not a real
 * airframe's numbers.
 *
 * Run() is the one override point: a module whose pilot genuinely differs (not just different
 * procedure numbers — a module with its own decision logic) overrides it, same override-point pattern
 * as FBAutopilot::Run/FBFlightControl::Run; FBF16Pilot (modules/f16/) composes this default and overrides
 * only the numeric hooks. */
#ifndef FBPILOT_H
#define FBPILOT_H

#include <optional>
#include "FBFlightPlan.h"
#include "FBRunway.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

class FBWorld;

/* The guidance the pilot hands to FBAutopilot. None = "don't touch the AP" — the module only calls the
 * matching FBAutopilot setter (SetManual/SetDirect) when a concrete mode is requested, so a
 * None-guidance tick changes nothing about whatever guidance is already running. */
enum class FBPilotGuidance { None, Manual, Direct };

/* One decision tick's output. Every airframe demand is std::optional: unset = "the pilot isn't
 * touching this control right now" (most ticks touch none of them), exactly mirroring a real pilot's
 * hands — the module only calls the matching FBAirframeControls setter when a field is present. */
struct FBPilotCommands {
  FBPilotGuidance Guidance = FBPilotGuidance::None;

  double TargetAltM = 0.0, TargetSpeedKt = 0.0;
  double TargetLatDeg = 0.0, TargetLonDeg = 0.0;   /* Direct target point (with TargetAltM/TargetSpeedKt) */
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
   * already needs internally, not a duplicate computation kept only for telemetry. */
  virtual FBPilotCommands Run(const FBState &state, const fb_fdm_state &fdm, const FBFlightPlan &plan,
                             const FBRunway *runway, const FBWorld *world, double dt);

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

private:
  void Transition(Phase p) { CurPhase = p; PhaseElapsedS = 0.0; }

  Phase CurPhase = Phase::Idle;
  double PhaseElapsedS = 0.0;
  int ActiveWpCache = -1;         /* telemetry cache, set in Run() (class banner) */
  double DistToWpCache = -1.0;
};

} // namespace FlightBox
#endif
