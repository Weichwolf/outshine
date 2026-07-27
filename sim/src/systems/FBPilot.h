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
 * Phase BFM (the fight): the one phase with no waypoint and no autopilot mode behind it. It regulates
 * against the LOCKED RADAR CONTACT — systems/FBBfmTrack turns successive contacts into an estimated
 * target position/velocity, and everything the fight needs (aspect, angle off the nose, closure, and the
 * extrapolation that survives a lost lock) comes out of that estimate. The pursuit curve follows the
 * geometry: lead to take angles away, lag to kill an overshoot and keep energy, pure in between; the
 * lift vector is rolled onto the resulting aim point and the pull is bounded by the g the current speed
 * can actually buy (BfmCornerSpeedKt & co. below). See Run()'s BFM section for the full derivation. It
 * is entered by mission declaration, not by sequencing, and it never leaves by itself.
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
#include "FBBfmTrack.h"
#include "FBCommandBus.h"
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
  /* Bfm is a TERMINAL phase in the sense the others are not: it is entered by mission declaration
   * (`set task bfm`, the module's own key) and left only by the run ending — a fight has no waypoint to
   * sequence to. It appends after Shutdown so no existing phase's telemetry string moves. */
  enum class Phase { Idle, Preflight, Takeoff, Climb, Route, Approach, Flare, Rollout, Shutdown, Bfm };
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
  virtual FBPilotCommands Run(const FBState &state, FBCommandBus &avionics,
                              const FBAirframeControls &airframe, const fb_fdm_state &st,
                              const FBFlightPlan &plan, const FBRunway *runway, double dt);

  /* ---- THE BRIEF: what this pilot was told to set up in the jet, and the ONLY thing it ever operates
   * an avionics box for. A value set here is not applied — it is something the pilot will go and ENTER
   * through the command bus once airborne, at the latency class of the control that carries it, and
   * which the jet may refuse. Unbriefed items are never touched, which is also why a mission that
   * briefs nothing issues no commands at all: a pilot does not retype numbers it was not given.
   * (Boot-time `set` lines that configure the airframe directly stay what they are — the spawn window,
   * before the pilot exists. What changed is that nothing may be reconfigured IN FLIGHT except through
   * the commands below.) */
  /* How long the pilot waits before trying a refused entry again. A rejected DED entry is a hand that
   * has to come off the stick and a head that has to go down again — retrying at the decision rate
   * (10 Hz) is not a pilot, it is a keyboard macro, and it would bury the command stream in noise. */
  static constexpr double kBriefRetryS = 2.0;

  void BriefAlowFt(double ft) { BriefAlowFt_ = ft; BriefAlowPending_ = true; }
  void BriefBingoLbs(double lbs) { BriefBingoLbs_ = lbs; BriefBingoPending_ = true; }
  void BriefMasterArm(bool arm) { BriefArm_ = arm; BriefArmPending_ = true; }
  void BriefWeapon(double sel) { BriefWeapon_ = sel; BriefWeaponPending_ = true; }
  /* THE RELEASE BRIEF: pickle at `atS` mission-elapsed seconds, one entry per store the brief calls
   * for, kept in the order they were briefed. Unlike the entries above this is not a value to enter
   * once and forget — it is an ACTION at a moment, so it carries its own time and is posted when that
   * moment arrives (Run()). Returns false if the brief is full (fixed capacity: no allocation in a
   * pilot). A refused release is NOT retried: the jet said no, and a pilot who keeps mashing the pickle
   * button is not flying the brief. */
  static constexpr int kMaxBriefedReleases = 8;
  bool BriefRelease(double atS);

  /* THE COUNTERMEASURE BRIEF: throw the selected dispense program at `atS` mission-elapsed seconds, one
   * entry per throw, in brief order. Identical in shape and reasoning to BriefRelease — an ACTION at a
   * moment rather than a value to enter — and identical in what it refuses to do: a dispense the jet
   * turned down is not retried, because a pilot who was told the magazine is empty does not keep
   * working the switch. A mission that flies SEMI/AUTO briefs nothing here: there the aircraft answers
   * its own warning receiver (systems/FBCountermeasureSystem). */
  static constexpr int kMaxBriefedDispenses = 8;
  bool BriefChaff(double atS);

  const char *TelemetryName() const override { return "pilot"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  /* The BFM picture + its scoreboard (systems/FBBfmTrack). Exposed because it is a SECOND telemetry
   * source the client registers on the bus — deliberately not folded into this class's own channels,
   * which sit in the middle of every existing telemetry.csv and must not move. */
  FBBfmTrack &BfmTrack() { return Bfm_; }
  const FBBfmTrack &BfmTrack() const { return Bfm_; }

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

  /* ---- BFM (the Bfm phase, see Run()'s BFM section) — the airframe's fight numbers ----
   * Generic placeholders again, and again NOT the override point: FBF16Pilot supplies the F-16's, every
   * one of them either measured against the vanilla model (`make test-corner`) or read off doc/f16/.
   * ENERGY. CornerSpeedKt is the speed the pilot manages TOWARD: the slowest speed that still buys the
   * airframe's best turn rate. CornerG is the load factor the airframe actually reaches there, and the
   * available-g estimate is CornerG scaled by (V/Vcorner)^2 (lift grows with dynamic pressure) capped at
   * MaxG — so the pull demanded is one the jet can produce, and the g loop never winds up asking for a
   * turn that is not on offer. Below MinSpeedKt the pilot is out of energy: the pull is cut to UnloadG
   * and the lift vector is biased below the target (trade height for speed), which is the one thing that
   * gets a slow fighter back into the fight. */
  virtual double BfmCornerSpeedKt() const { return 300.0; }
  virtual double BfmCornerG() const { return 4.0; }
  virtual double BfmMaxG() const { return 6.0; }
  virtual double BfmMinSpeedKt() const { return 220.0; }
  virtual double BfmUnloadG() const { return 3.0; }
  /* GEOMETRY. The control position: behind him, close, nose on, lock held. */
  virtual double BfmControlMinNm() const { return 0.5; }
  virtual double BfmControlMaxNm() const { return 1.5; }
  virtual double BfmControlAspectDeg() const { return 30.0; }
  virtual double BfmControlAtaDeg() const { return 30.0; }
  /* PURSUIT SELECTION. Lag when the overtake would throw the nose out in front (inside LagRangeNm with
   * more than MaxClosureKt of closure, or simply too close); lead while the angles are still the
   * problem (aspect beyond LeadAspectDeg, or further out than LeadRangeNm); pure in between. */
  virtual double BfmClosureGainKtPerNm() const { return 120.0; }   /* the closure SCHEDULE's slope */
  virtual double BfmMaxClosureKt() const { return 200.0; }
  virtual double BfmLeadAspectDeg() const { return 45.0; }
  virtual double BfmLeadRangeNm() const { return 3.0; }
  virtual double BfmLeadMaxS() const { return 4.0; }     /* cap on the collision-course lead time */
  virtual double BfmLagTimeS() const { return 2.5; }     /* how far behind him the lag point sits */
  virtual double BfmYoYoHeightM() const { return 400.0; }/* the high yo-yo's height at full excess closure */
  /* SEARCH. How long a coasting track is chased with the nose alone before the scan starts weaving, and
   * the weave itself — the pattern that walks the (body-fixed) radar box across the uncertainty. */
  virtual double BfmScanAfterS() const { return 3.0; }
  /* The weave has to stay GENTLE: its own heading rate (2*pi*A/T) is a steering error like any other, so
   * a fast, wide scan would have the pilot flying a hard turn chasing its own search pattern instead of
   * looking. A/T here is a few deg/s — a lazy S, which is what a scan is. */
  virtual double BfmScanAmplitudeDeg() const { return 8.0; }
  virtual double BfmScanPeriodS() const { return 30.0; }
  virtual double BfmFloorFt() const { return 2000.0; }   /* AGL below which the pull is biased up */

private:
  /* One decision tick of cockpit work: posts the next briefed item that still needs entering. A
   * bus-level rejection (channel busy, or the manoeuvre gate — the pilot is flying, not typing) leaves
   * the item pending and it is retried; anything that reaches the owning box is final, because a pilot
   * who was told "no" by the jet does not retype the same entry forever. */
  void EnterBriefedItems(FBCommandBus &avionics);

  /* Posts every briefed release whose moment has come (see BriefRelease). */
  void ReleaseBriefedStores(FBCommandBus &avionics);
  /* ...and every briefed countermeasure dispense (see BriefChaff). */
  void DispenseBriefedCm(FBCommandBus &avionics);

  /* The Bfm phase's body — one decision tick of the fight (see Run()'s BFM section). Separate because
   * it is the only phase with an inner control law of its own rather than a target for the autopilot. */
  FBPilotCommands BfmCommands(const FBState &state, const fb_fdm_state &st, double dt);
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

  FBBfmTrack Bfm_;                /* the fight's picture + scoreboard; inert outside the Bfm phase */
  double TimeS_ = 0.0;            /* the pilot's own clock — the tracker stamps looks in absolute time */
  double BfmGIterm_ = 0.0;        /* the g loop's integrator (see BfmCommands) */
  double BfmSearchHdgDeg_ = 0.0;  /* the cold search's anchored heading + altitude (see BfmCommands) */
  double BfmSearchAltM_ = 0.0;
  bool   BfmSearchAnchored_ = false;

  /* The briefed releases, in brief order, consumed front to back — ReleaseNext_ is how many have been
   * pickled, so the array is never rewritten and the sequence is exactly what the mission declared. */
  double ReleaseAtS_[kMaxBriefedReleases]{};
  int    ReleaseCount_ = 0, ReleaseNext_ = 0;
  double DispenseAtS_[kMaxBriefedDispenses]{};
  int    DispenseCount_ = 0, DispenseNext_ = 0;

  double BriefAlowFt_ = 0.0, BriefBingoLbs_ = 0.0, BriefWeapon_ = 0.0;
  bool   BriefArm_ = true;
  bool   BriefAlowPending_ = false, BriefBingoPending_ = false;
  bool   BriefArmPending_ = false, BriefWeaponPending_ = false;
  double BriefNextTryS_ = 0.0;
};

} // namespace FlightBox
#endif
