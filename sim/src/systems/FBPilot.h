/* FlightBox — FBPilot: the mission-level brain ABOVE the existing stack (core/ architecture banner) —
 * FBFlightControl (100 Hz, the hands) and FBAutopilot (the manoeuvre executor) stay untouched; FBPilot
 * decides WHERE the aircraft should go and hands that down once per ~10 Hz decision tick as
 * FBPilotCommands: a guidance request for FBAutopilot plus airframe demands for FBAirframeControls. The
 * pilot does not know whose airframe it flies — Ownship today, AI units later, same interface.
 *
 * The phase machine (taxi/takeoff/climb/route/approach/land, doc/f16/procedures-*.md) is the procedure
 * SKELETON this round: the enum and the accessors exist, but Run() always returns a neutral
 * FBPilotCommands (Guidance::None, every airframe demand unset) regardless of phase — composing this
 * system into a module's Run() therefore changes NOTHING about existing flight (Bit-Identität). Later
 * rounds fill in the per-phase behaviour; SetPhase is the boot/test hook that reaches the machine before
 * that behaviour exists.
 *
 * Run() is the one override point: a module whose pilot genuinely differs (not just different
 * procedure numbers — a module with its own decision logic) overrides it, same override-point pattern
 * as FBAutopilot::Run/FBFlightControl::Run; FBF16Pilot (modules/f16/) is the F-16's default-composing
 * skeleton for round A. */
#ifndef FBPILOT_H
#define FBPILOT_H

#include <optional>
#include "FBFlightPlan.h"
#include "FBRunway.h"
#include "FBState.h"
#include "jsbsim_adapter.h"

namespace FlightBox {

class FBWorld;

/* The guidance the pilot hands to FBAutopilot. None = "don't touch the AP" — the module only calls the
 * matching FBAutopilot setter (SetLoiter/SetLowLevel/SetManual) when a concrete mode is requested, so a
 * None-guidance tick changes nothing about whatever guidance is already running. */
enum class FBPilotGuidance { None, Manual, Loiter, LowLevel };

/* One decision tick's output. Every airframe demand is std::optional: unset = "the pilot isn't
 * touching this control right now" (most ticks touch none of them), exactly mirroring a real pilot's
 * hands — the module only calls the matching FBAirframeControls setter when a field is present. */
struct FBPilotCommands {
  FBPilotGuidance Guidance = FBPilotGuidance::None;

  double TargetHeadingDeg = 0.0, TargetAltM = 0.0, TargetSpeedKt = 0.0;   /* LowLevel: heading/AGL/speed */
  double LoiterLatDeg = 0.0, LoiterLonDeg = 0.0, LoiterRadiusM = 0.0;     /* Loiter target */
  int    LoiterDir = 1;
  double ManualRoll = 0.0, ManualPitch = 0.0, ManualYaw = 0.0, ManualThr = 0.0;   /* Manual pass-through */

  std::optional<bool>   GearDown;
  std::optional<double> Speedbrake;                       /* 0..1 */
  std::optional<double> WheelBrakeLeft, WheelBrakeRight;   /* 0..1 */
  std::optional<double> NosewheelSteer;                    /* -1..1 */
  std::optional<bool>   EngineStart;                       /* true = start commanded, false = cutoff */
};

class FBPilot {
public:
  enum class Phase { Idle, Preflight, Takeoff, Climb, Route, Approach, Flare, Rollout, Shutdown };

  FBPilot() = default;
  virtual ~FBPilot() = default;

  Phase GetPhase() const { return CurPhase; }
  void  SetPhase(Phase p) { CurPhase = p; }

  /* The one override point (see the class banner). `plan` is the mission's waypoint chain, `runway`
   * the assigned runway for takeoff/landing phases (nullptr while none is assigned). */
  virtual FBPilotCommands Run(const FBState &state, const fb_fdm_state &fdm, const FBFlightPlan &plan,
                             const FBRunway *runway, const FBWorld *world, double dt);

private:
  Phase CurPhase = Phase::Idle;
};

} // namespace FlightBox
#endif
