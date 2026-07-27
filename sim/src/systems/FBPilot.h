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
 * waypoint at ITS own alt/speed — and, from the SECOND waypoint on, on the LEG from the one before it
 * rather than on the bearing to it: two declared fixes are a track, and a track is flown, not chased
 * (FBAutopilot's banner has the measurement). The first waypoint of a plan has no declared inbound
 * track and is flown as a bearing, unchanged;
 * FBFlightPlan's own WP_REACHED sequencing, driven by the caller, advances
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
 * Phase ATTACK (the air-to-ground delivery, doc/f16/weapons.md §2.5): the only phase whose decision is
 * a MOMENT rather than a trajectory. It has three parts and the middle one lasts one tick:
 *   RUN-IN   FBAutopilot::Direct to the active waypoint at ITS declared altitude and speed — i.e. a
 *            level laydown attack, flown as a TRACK: the point the pass was commenced from is anchored
 *            once and the leg to the target is held from there (SetDirectLeg), which is what an attack
 *            run is. That is a deliberate profile choice and the reason is the guidance this simulator
 *            has: Direct holds an altitude, so a straight, stable, level run-in is something it flies
 *            EXACTLY, while a 20-30 degree dive would be the pilot fighting its own altitude hold all
 *            the way down the chute. A level delivery is also what makes the release cue the only
 *            variable under test: the run-in is repeatable to the metre, so every metre of miss belongs
 *            to the computation or to the moment, not to the flying — and holding the track rather than
 *            the bearing is what makes that true ACROSS the run-in too (measured: 31.6 m of lateral
 *            miss became 10.6 m, with the along-track half unchanged).
 *   RELEASE  one pickle, over the command bus, on the fire control's own cue (the FBFireControlBlock's
 *            air-to-ground fields — modules/f16/FBF16FireControl solves them, this class only reads
 *            them, like every other instrument). WHICH cue is the briefed delivery mode: CCRP releases
 *            when the solution cue passes (time-to-release <= 0), CCIP additionally insists that the
 *            predicted impact point is laterally ON the target (|AgCrossErrM| inside the tolerance),
 *            which is what a pilot with the pipper in view can see and a countdown cannot. Both are gated
 *            on the arming margin, so a release that would arrive as a dud is not made.
 *   EGRESS   a check turn away from the run-in track with a climb, held for a fixed time, then back to
 *            Route. It is not decoration: a level delivery flies the aircraft over its own detonation,
 *            and turning away is what the profile is.
 * Every number in it is an airframe/pilot hook below (AttackReleaseBiasS & co.), so a mission can bias
 * the release moment by a declared number of seconds — which is how the computation is proved to be
 * doing something: the same profile released two seconds late lands a groundspeed's worth of metres long.
 *
 * Phase INTERCEPT (the beyond-visual-range engagement): BFM's opposite in every respect that matters.
 * BFM is flown with the nose and the lock never leaves the target; an intercept is flown with the
 * SENSOR and the whole art is deciding when to point it, when to shoot, how long to keep supporting the
 * shot and when to stop. It is therefore a small state machine of its own (systems/FBEngagement's
 * FBEngageState, which also records what it did):
 *   SEARCH   fly the briefed vector, radar in a SEARCH mode, antenna elevation pointed at the altitude
 *            band the target is expected in, and DO NOT LOCK. A lock is a personal warning to the
 *            aircraft it is pointed at (systems/FBRwrSystem), so it is spent as late as possible.
 *   CLOSING  a contact is on the scope: close on it, still without locking, still searching.
 *   ATTACK   inside the briefed lock range: designate (TMS forward), read the launch zone the fire
 *            control publishes, and pull the trigger when the geometry is right — inside Rtr (the range
 *            from which the round arrives even if he turns and runs) rather than at Raero, which is a
 *            shot he can outrun, and with the target inside the seeker's own acquisition cone.
 *   SUPPORT  a round is in the air. Hold the lock — an AIM-120 flies its midcourse on the launcher's
 *            uplink — but CRANK: turn away until the target sits at the edge of what the antenna can
 *            still follow. That buys separation for free, because the lock is what costs, not the nose.
 *            When the fire control's own countdown says the seeker has taken over, the shot no longer
 *            needs him.
 *   DEFEND   somebody has a firing solution on this aircraft. Turn ACROSS his line of sight — that is
 *            what puts own radial velocity inside a pulse-Doppler set's clutter filter — and throw
 *            chaff, which is worth nothing without exactly that manoeuvre (systems/FBRadarSystem's
 *            notch). Both go out over the command bus, after a human's reaction time.
 *   ABORT    nothing left to shoot with, or the fight has closed inside the range this phase is for.
 * The phase reads the RADAR, the RWR and the FIRE-CONTROL blocks and nothing else, and it operates the
 * jet exclusively through FBCommandBus — radar mode, antenna elevation, designation, trigger, chaff.
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
#include "FBEngagement.h"
#include "FBFlightPlan.h"
#include "FBPilotTuning.h"
#include "FBRunway.h"
#include "FBState.h"
#include "FBStore.h"
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
  /* THE LEG the Direct target sits at the end of, when the pilot is flying one (FBAutopilot::
   * SetDirectLeg): the guidance then holds that TRACK instead of the bearing to the point. Unset is not
   * a missing value, it is the statement "there is no line here" — an intercept, a fight, a search and a
   * break turn all steer at a point and are meant to. Not a std::optional pair like the airframe demands
   * below because it is not a control the pilot may or may not touch: it is what the pilot KNOWS about
   * where it is going, and the flag is that knowledge. */
  bool   HaveLeg = false;
  double LegLatDeg = 0.0, LegLonDeg = 0.0;         /* the leg's origin fix */
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
   * sequence to. It appends after Shutdown so no existing phase's telemetry string moves.
   *
   * Attack is the AIR-TO-GROUND delivery (see the Run() section): entered by declaration like the two
   * above, but unlike them it ENDS — a bombing pass has a run-in, one moment, and a way out, after
   * which the jet is back on its route. Appended last, same rule. */
  enum class Phase { Idle, Preflight, Takeoff, Climb, Route, Approach, Flare, Rollout, Shutdown, Bfm,
                     Intercept, Attack };
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

  /* THE ATTACK BRIEF: which delivery mode the pass is flown in (core/FBStore.h's FBDeliveryMode). Unlike
   * the items above it is not entered into a box over the bus — it is what the PILOT decides to act on,
   * and the fire control publishes both cues regardless. The module sets the same mission line on its
   * own fire control too, so the release RECORD says which cue the round was let go on. */
  void BriefAttack(FBDeliveryMode m) { AtkMode_ = m; }
  FBDeliveryMode AttackMode() const { return AtkMode_; }

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

  /* The intercept's state machine + its debrief (systems/FBEngagement), a THIRD telemetry source for
   * the same reason the BFM one is a second: appending channels never moves a column that has already
   * been measured. */
  FBEngagement &Engagement() { return Eng_; }
  const FBEngagement &Engagement() const { return Eng_; }

  /* THE VARIANT (systems/FBPilotTuning): the decision numbers a mission may override, so that one
   * pilot differs from another by a line in a `.fbm` file instead of by a subclass. The module hands
   * its own `set pilot_*` lines here (FBModule::ApplySetup) during the spawn window and passes the
   * refusal on as a mission FAIL; every entry the mission leaves alone stays this pilot's own number,
   * so a mission that tunes nothing is unaffected by the existence of this table. Write-only from
   * outside: the table is read exclusively through Tuned() below, in the decision path itself. */
  bool ApplyTuning(const std::string &key, double value) { return Tune_.Set(key, value); }

protected:
  /* The turn rate this airframe's own BFM numbers imply — g*sqrt(n^2-1)/V at corner. DERIVED ONCE AND
   * USED TWICE, which is why it is a method rather than two constants: it is the fastest this jet can
   * swing its nose after a moving gun solution, and it is also what the pilot assumes the OTHER jet can
   * do when it works out where a target it lost could have got to (FBTrackDatum). For the F-16's
   * measured corner (380 kt / 5.6 g) it comes out at 15.8 deg/s against the 16.2 deg/s `make
   * test-corner` measures directly — the derivation checks out against the model. */
  double CornerTurnRateDegS() const;

  /* The one reader the decision code uses: this variant's value for `p` if the mission set one, else
   * the number the caller passed in — which is always this pilot's own hook, so the airframe's numbers
   * stay in the airframe's class and the override stays sparse and visible at the point of use. */
  double Tuned(FBPilotParam p, double own) const { return Tune_.Or(p, own); }

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
  /* THE TRIGGER: how long one squeeze lasts. The command bus enforces 0.5 s between two actions on the
   * same switch (core/FBCommandBus.h), so a burst of exactly that length is continuous fire for as long
   * as the funnel holds, and anything longer is a deliberately committed burst. */
  virtual double BfmGunBurstS() const { return 0.5; }
  /* How far off the required bore the nose may be and still be a TRACKING problem rather than a turning
   * one — the boundary between flying the funnel and flying the pursuit law (see BfmCommands). */
  virtual double BfmGunTrackMaxErrDeg() const { return 20.0; }
  /* What fraction of the funnel's own tolerance the pilot insists on before squeezing (see BfmGunfire).
   * 0.35 puts the pattern inside the target's fuselage rather than anywhere across its span. */
  virtual double BfmGunFireTolFrac() const { return 0.35; }

  /* ---- INTERCEPT (the Intercept phase, see the class banner) — this jet's BVR numbers ----
   * Generic placeholders again, and again not the override point: FBF16Pilot supplies the F-16's, each
   * of them derived from the APG-68's geometry or from the AIM-120's launch zone rather than picked.
   *
   * THE RADAR MODE THE SEARCH IS FLOWN IN is a module ordinal (FBRadarBlock::ModeOrdinal is documented
   * as "the module's own mode label, not logic"), so the generic layer cannot name it — it can only ask
   * for it. -1 = "this module has no separate non-locking search mode, leave the set alone", which is
   * also what keeps a module that never defined one from having its radar switched to mode -1. */
  virtual int    SearchRadarModeOrdinal() const { return -1; }
  virtual double InterceptSpeedKt() const { return 300.0; }
  /* WHERE THE LOCK IS SPENT. Far enough out that the single-target track has settled and the fire
   * control has a launch zone before the shot range arrives; close enough that the warning it gives the
   * target is not a free half-minute of preparation. */
  virtual double InterceptLockRangeNm() const { return 20.0; }
  /* THE SHOT. Taken at RtrFactor times Rtr — the range from which the round still arrives if the target
   * reverses at launch — and only with the target inside ShotAtaDeg of the nose, which is what a
   * rail-launched round can still pull onto. RtrFactor is a fraction of a number the fire control
   * computes per shot, not a range of its own, so it stays right when the geometry changes. */
  virtual double InterceptShotRtrFactor() const { return 1.0; }
  virtual double InterceptShotAtaDeg() const { return 30.0; }
  virtual double InterceptShotSpacingS() const { return 12.0; }   /* between two shots at one target */
  /* THE CRANK: how far off the nose the target is allowed to sit while the shot is being supported.
   * It IS the antenna's gimbal limit minus the margin a manoeuvring target needs — turn further and the
   * track breaks, which is the one thing the support phase must not do. */
  virtual double InterceptCrankAtaDeg() const { return 45.0; }
  /* Below this the engagement is no longer beyond visual range: an intercept has failed to do its job
   * and pressing on is a merge, not a shot. */
  virtual double InterceptAbortRangeNm() const { return 5.0; }

  /* ---- ATTACK (the Attack phase, see the class banner) — this jet's air-to-ground delivery numbers ----
   * Generic placeholders again, and again not the override point. Everything about WHERE the round goes
   * is the fire control's; these are only about the pilot's own hands and its way out.
   *
   * THE BIAS is the phase's measurement instrument, and it is a number rather than a switch on purpose:
   * a delivery released `bias` seconds after the computed cue lands one groundspeed-second per second
   * long, so a mission can declare a wrong release and the resulting miss is the computation's own
   * answer to "does this do anything". 0 = release on the cue, which is every real pass. */
  virtual double AttackReleaseBiasS() const { return 0.0; }
  /* HOW FAR BESIDE THE TARGET the predicted impact point may sit and still be "pipper on": the
   * ACROSS-track half of the aiming error at the moment the cue passes. Only the across half, because the
   * along half is what the cue itself is about — it is deliberately non-zero when the button goes down.
   * It is the judgement a CCRP countdown cannot make, and the reason the two modes are not one release. */
  virtual double AttackCcipTolM() const { return 60.0; }
  /* THE WAY OUT: how far off the run-in track to break, how high to go, how far ahead the turn is aimed
   * and how long it is held before the jet is back on its route. */
  virtual double AttackEgressTurnDeg() const { return 120.0; }
  virtual double AttackEgressClimbM() const { return 500.0; }
  virtual double AttackEgressRangeM() const { return 12000.0; }
  virtual double AttackEgressS() const { return 25.0; }
  /* THE DEFENCE: how far across the threat's line of sight to turn (90 deg = pure beam, where own
   * radial velocity is zero and a pulse-Doppler set cannot tell the aircraft from a chaff cloud) and how
   * often to keep throwing while it lasts. */
  virtual double InterceptBeamOffsetDeg() const { return 90.0; }
  virtual double InterceptChaffIntervalS() const { return 3.0; }
  /* How long a defensive reaction is held after the last warning: a beam manoeuvre abandoned the instant
   * the symbol drops is one that never got out of the notch, and the receiver goes quiet exactly BECAUSE
   * the manoeuvre worked. */
  virtual double InterceptDefendHoldS() const { return 12.0; }

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
  FBPilotCommands BfmCommands(const FBState &state, FBCommandBus &avionics, const fb_fdm_state &st,
                              double dt);
  /* The fight's trigger finger (see the .cpp). Separate from the flying above for the same reason the
   * intercept's cockpit work is: operating a weapon is not steering. */
  void BfmGunfire(const FBState &state, FBCommandBus &avionics);
  /* The Attack phase's body (class banner). Separate for the same reason the two above are: it decides
   * for itself what the aircraft is doing and it operates a weapon as part of flying. */
  FBPilotCommands AttackCommands(const FBState &state, FBCommandBus &avionics, const fb_fdm_state &st,
                                 const FBFlightPlan &plan);
  /* The Intercept phase's body (class banner). Separate for the same reason BfmCommands is: it is the
   * only other phase that decides for itself what the aircraft is doing, and the only one that operates
   * avionics as part of flying rather than as briefed cockpit work. */
  FBPilotCommands InterceptCommands(const FBState &state, FBCommandBus &avionics,
                                    const fb_fdm_state &st, const FBFlightPlan &plan, double dt);
  /* ONE cockpit action per decision tick, in priority order, at a human's working rate (see
   * kInterceptActionS): the defensive ones first, because the jet being shot at is not editing a radar
   * mode. Returns true if something was posted. */
  bool InterceptCockpit(const FBState &state, FBCommandBus &avionics, int designateTrack, bool wantShot,
                        bool wantChaff, double wantElDeg);
  /* IS THERE STILL A FIGHT IN THIS JET? The honest half of the re-attack decision (see the Intercept
   * section of Run()): weapons on the racks, fuel above the committed threshold, and a sensor that can
   * find him again. Any of the three missing and turning back is not courage, it is a jet with nothing
   * to do arriving inside somebody else's launch zone. Reads only blocks — stores, warnings, radar. */
  bool CanPressOn(const FBState &state) const;
  /* The search weave's own offset, in degrees, anchored to the moment the search started rather than to
   * absolute sim time (see Run()'s BFM section) and widened to whatever the datum's uncertainty demands. */
  double SearchWeaveDeg(const FBTrackDatum &datum, bool searching);
  void Transition(Phase p) { CurPhase = p; PhaseElapsedS = 0.0; }

  /* Runway-relative along/across-track (m), the SAME axis convention FBMissionMonitor::OnRunway and
   * FBAutopilot::SetCourse use (along=0 at the threshold, +down the runway; +across = right of course) —
   * shared by Takeoff's ground-steering law and Rollout's reuse of it below. */
  /* THE INBOUND TRACK OF THE ACTIVE WAYPOINT, if the mission declared one. A route leg is two DECLARED
   * fixes and nothing else: the track to the FIRST waypoint of a plan does not exist — the aircraft goes
   * there from wherever it happens to be, which is a bearing, and inventing a line out of its spawn
   * position would turn a defender told to circle one point into a jet that flies at it once and leaves
   * (missions/bfm-basic.fbm). */
  static void SetLegFromPlan(FBPilotCommands &c, const FBFlightPlan &plan);

  static void RunwayAxis(const FBRunway &rwy, double lat, double lon, double &alongM, double &acrossM);
  double NosewheelSteerCmd(const FBRunway &rwy, double lat, double lon, double yawDeg) const;
  double PitchHoldStick(double targetDeg, double pitchDeg, double qDegS, double stickMax) const;

  Phase CurPhase = Phase::Idle;
  double PhaseElapsedS = 0.0;
  int ActiveWpCache = -1;         /* telemetry cache, set in Run() (class banner) */
  double DistToWpCache = -1.0;

  FBBfmTrack Bfm_;                /* the fight's picture + scoreboard; inert outside the Bfm phase */
  FBEngagement Eng_;              /* the intercept's state + debrief; inert outside the Intercept phase */
  FBPilotTuning Tune_;            /* this pilot's variant — empty unless the mission set one */

  /* ---- the Intercept phase's own memory (systems/FBEngagement holds the record; this is the state the
   * decisions are taken from) ---- */
  FBEngageState EngState_ = FBEngageState::Idle;
  int    IntTrack_ = 0;             /* the contact number being worked, 0 = none */
  double IntBriefAltM_ = 0.0;       /* the altitude band the search covers; the brief's, else own */
  double IntBriefHdgDeg_ = 0.0;     /* the vector flown while nothing is on the scope */
  bool   IntAnchored_ = false;
  double IntLastActionS_ = -1e9;    /* one cockpit action at a time (kInterceptActionS) */
  double IntLastShotS_ = -1e9;
  double IntNextShotS_ = -1e9;      /* not before the round already in the air has had its chance */
  double IntLastChaffS_ = -1e9;
  double IntLockSinceS_ = -1.0;     /* when the current single-target track started (see the settle) */
  double IntCmdElDeg_ = 0.0;        /* the antenna elevation this pilot has asked for */
  bool   IntCmdMode_ = false;       /* the search mode has been selected once */
  int    IntSeenReleases_ = 0;      /* the stores block's own release count, to notice a shot leaving */
  int    IntSeenChaff_ = 0;
  double IntThreatLastS_ = -1e9;    /* last tick a warning demanded an answer (the defence's hold) */
  double IntDefendCueS_ = -1.0;     /* when it started demanding one — the reaction time's zero */
  double IntCrankSign_ = 1.0;       /* which way the crank turn goes, decided once per support entry */
  bool   IntHaveCrankSign_ = false;
  double TimeS_ = 0.0;            /* the pilot's own clock — the tracker stamps looks in absolute time */
  double BfmGIterm_ = 0.0;        /* the g loop's integrator (see BfmCommands) */
  double BfmRollCmdPrev_ = 0.0;   /* the roll governor's own last command (see BfmCommands) */
  double BfmSearchHdgDeg_ = 0.0;  /* the cold search's anchored heading + altitude (see BfmCommands) */
  double BfmSearchAltM_ = 0.0;
  bool   BfmSearchAnchored_ = false;
  double ScanSinceS_ = 0.0;       /* when the current scan started — the weave's phase zero */
  bool   ScanRunning_ = false;
  /* ---- the Attack phase's own memory ---- */
  FBDeliveryMode AtkMode_ = FBDeliveryMode::Ccip;
  bool   AtkReleased_ = false;    /* the pass is spent: one pickle per declared attack */
  bool   AtkInRangeSeen_ = false; /* the cue has been positive at least once (see AttackCommands) */
  /* WHERE THE PASS WAS COMMENCED FROM — the run-in's own leg origin (see AttackCommands). An attack run
   * is flown on a track that is established once, when the jet rolls out on it, and held to the release;
   * the mission declares no initial point, so the point the run-in started from IS the initial point. */
  bool   AtkHaveRunIn_ = false;
  double AtkRunInLatDeg_ = 0.0, AtkRunInLonDeg_ = 0.0;
  double AtkEgressUntilS_ = 0.0;  /* the break turn's own clock, absolute */
  double AtkEgressLatDeg_ = 0.0, AtkEgressLonDeg_ = 0.0, AtkEgressAltM_ = 0.0;

  double GunNextS_ = -1e9;        /* not before the burst already commanded has finished */
  bool   GunTracking_ = false;    /* flying the funnel (see BfmCommands) — logged on the transition */
  double GunPrevErrDeg_ = 0.0, GunPrevS_ = 0.0;   /* the aiming error's rate (see BfmGunfire's lead) */
  bool   GunHaveErr_ = false;
  /* The gun TRACKING loop's rate term (see BfmCommands): the required bore as a WORLD direction, and
   * that direction's own rate of turn, differenced across decision ticks and filtered. World-referenced
   * on purpose — a body-referenced difference would be measuring this jet's pull, not the solution's. */
  double GunLeadPrevE_ = 0.0, GunLeadPrevN_ = 0.0, GunLeadPrevU_ = 0.0, GunLeadPrevS_ = 0.0;
  double GunLeadRateE_ = 0.0, GunLeadRateN_ = 0.0, GunLeadRateU_ = 0.0;
  double GunTrackIAz_ = 0.0, GunTrackIEl_ = 0.0;   /* the tracking loop's integrator, per axis */
  bool   GunHaveLead_ = false;

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
