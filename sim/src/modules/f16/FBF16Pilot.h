/* FlightBox — FBF16Pilot: the F-16's pilot numbers (systems/FBPilot's virtual hooks, not its Run() —
 * the takeoff phase machine itself is generic, see FBPilot's banner). Every value here traces to
 * doc/f16/procedures-takeoff-taxi.md:
 *   - RotationSpeedKt: the weight/Vr table (128..198 KIAS across 20,000..44,000 lb), interpolated —
 *     "Takeoff speed scales with weight ... rotation is lift-limited at the takeoff AoA".
 *   - RotationLeadKt: "Afterburner: begin pull ~15 kts below takeoff speed" (we always take off in AB,
 *     see TakeoffThrottleNorm).
 *   - RotationPitchDeg: "Rotate to 8-12 deg pitch takeoff attitude" — 10 deg, the middle of that band.
 *   - GearUpLimitKt: "Retract gear before 300 kts (higher speeds risk gear-door structural damage)".
 *   - ClimbSpeedKt: no single doc figure for the climb-out leg; 350 kt matches the mission's own
 *     climb-out waypoint target, a conservative sub-corner-speed schedule.
 *   - TakeoffThrottleNorm: "Full Afterburner (heavy load / short runway)" — 1.0 (fcs/throttle-cmd-norm's
 *     top of range is the AB detent in the vanilla model).
 *
 * Measured, accepted model characteristic (CLAUDE.md Prinzip 5 -- the vanilla model, not the published
 * real-jet number, is the reference): a full-aft-stick rotation from brake release produces almost no
 * pitch response below ~150 KCAS (elevator control power scales with dynamic pressure, i.e. ~V^2) --
 * this vanilla f16.xml model's clean/near-empty-fuel (~20,600 lb) configuration only generates enough
 * lift to fly around 170 KCAS, well above this weight's 128-130 kt Vr-table entry, and the achievable
 * pitch attitude at that natural liftoff point tops out near 5 deg regardless of when the rotation pull
 * starts. RotationSpeedKt/RotationPitchDeg stay doc-faithful (the procedure -- pull ~15 kt below the
 * table Vr, target 10 deg -- is still correct technique); the mismatch is the model's low-speed lift/
 * elevator authority, not this class's numbers.
 *
 * Landing numbers (Phase 3, doc/f16/procedures-landing.md), gear-down/~40% fuel: GlidepathAngleDeg (3
 * deg, "standard glidepath angle", doc/f16/navigation-ils.md), FlareTargetPitchDeg/AerobrakePitchDeg/
 * AerobrakeSpeedKt straight off the guide's Short-Final/Roll-Out steps (touchdown <=13 deg AoA, hold
 * ~13 deg nose-up until ~100 kt) with a few degrees of margin off the 13/15 deg numbers against
 * FBFlightMonitor's 15-deg attitude-contact K.O. ApproachSpeedKt has no single doc CAS figure ("control
 * AoA with throttle... target 11 deg AoA" is the procedure, not a speed) -- MEASURED against the vanilla
 * model instead: trimmed level, gear down, ~40% fuel, the model holds 11.0 deg AoA at 164.9 KCAS
 * (gym calibration sweep, doc/f16/procedures-landing.md's 11 deg on-speed target); this class commands
 * that CAS and lets the existing throttle-PI (FBFlightControl::F16) hold it, an open-loop stand-in for a
 * closed AoA loop that is faithful to the model's own trim curve rather than a copied real-jet number.
 *
 * BFM numbers (systems/FBPilot's Bfm phase), MEASURED — `make test-corner` (app/FBTestCornerSpeed.cpp)
 * sweeps entry speed against instantaneous turn rate on THIS model, 85 deg bank, full aft stick through
 * the model's own FLCS, 20 kt steps from 180 to 620 KCAS at 5,000 m:
 *     280 kt -> 12.8 deg/s @ 3.2 g | 340 kt -> 14.9 @ 4.6 | 380 kt -> 16.2 @ 5.6 (peak)
 *     420 kt -> 14.7 @ 5.8         | 500 kt -> 11.7 @ 5.6 | 620 kt -> 12.9 @ 7.5
 * so BfmCornerSpeedKt = 380 and BfmCornerG = 5.6, and 300 kt is where the rate has fallen ~17 % below
 * the peak — the pilot's BfmMinSpeedKt floor. Two accepted model properties (CLAUDE.md Prinzip 5) show
 * up in that table and are NOT defects to be tuned away: the vanilla f16.xml FLCS is a pitch-RATE
 * command (f16.xml's Pitch channel differences the stick against 6.2*q and only 0.02*nz), so full aft
 * stick buys ~5.6 g at corner rather than the doc's 9 g structural limit, and the resulting best turn
 * rate is ~16 deg/s rather than the real jet's ~20+. The measured corner nevertheless lands inside
 * doc/f16/aerodynamics-performance.md's published 330-440 KCAS corner PLATEAU, which is the strongest
 * cross-check available for a number the doc deliberately does not tabulate. */
#ifndef FBF16PILOT_H
#define FBF16PILOT_H

#include "FBPilot.h"

namespace FlightBox {

class FBF16Pilot : public FBPilot {
protected:
  double RotationSpeedKt(double grossWeightLbs) const override;
  double RotationLeadKt() const override { return 15.0; }
  double RotationPitchDeg() const override { return 10.0; }
  double GearUpLimitKt() const override { return 300.0; }
  double ClimbSpeedKt() const override { return 350.0; }
  double TakeoffThrottleNorm() const override { return 1.0; }

  double ApproachSpeedKt() const override { return 165.0; }
  double GlidepathAngleDeg() const override { return 3.0; }
  double ApproachSpeedbrakeNorm() const override { return 0.5; }
  double FlareStartAglFt() const override { return 50.0; }
  double FlareTargetPitchDeg() const override { return 12.5; }
  double AerobrakePitchDeg() const override { return 12.0; }
  double AerobrakeSpeedKt() const override { return 100.0; }
  double RolloutBrakeNorm() const override { return 0.8; }

  /* BFM (systems/FBPilot's Bfm phase). Corner speed and the g that goes with it are MEASURED against the
   * vanilla model (`make test-corner`, app/FBTestCornerSpeed.cpp) — see this class's banner for the
   * sweep and why the model's own number, not the real jet's plateau, is what the pilot flies. */
  double BfmCornerSpeedKt() const override { return 380.0; }
  double BfmCornerG() const override { return 5.6; }
  double BfmMaxG() const override { return 9.0; }
  double BfmMinSpeedKt() const override { return 300.0; }
  double BfmUnloadG() const override { return 3.0; }
  double BfmControlMinNm() const override { return 0.5; }
  double BfmControlMaxNm() const override { return 1.5; }
  double BfmControlAspectDeg() const override { return 30.0; }
  double BfmControlAtaDeg() const override { return 30.0; }
  double BfmClosureGainKtPerNm() const override { return 120.0; }
  double BfmMaxClosureKt() const override { return 200.0; }
  /* MEASURED on this model, same discipline as the corner numbers: every telemetry sample of the gun
   * approach sweep taken with the throttle at idle, the speedbrake out, inside 15 deg of bank and at
   * 1 g, with the flight-path component of gravity removed — 238 samples at 4,000 m between 325 and
   * 400 KCAS, median 2.39 m/s^2, p10 1.64, p90 3.80. The MEDIAN is the number, i.e. the authority this
   * airframe has rather than the one it is guaranteed: the conservative p10 was measured too and it
   * throttles the whole approach (over the sixteen-approach sweep, 11 kills against 14 and 21 s less
   * time in the funnel), because every knot the schedule gives away at long range is a knot the pursuer
   * has to make up inside the merge. */
  double BfmBrakeMs2() const override { return 2.4; }
  double BfmLeadAspectDeg() const override { return 45.0; }
  double BfmLeadRangeNm() const override { return 3.0; }
  double BfmLeadMaxS() const override { return 4.0; }
  double BfmLagTimeS() const override { return 2.5; }
  double BfmYoYoHeightM() const override { return 600.0; }
  double BfmScanAfterS() const override { return 3.0; }
  double BfmScanAmplitudeDeg() const override { return 8.0; }    /* a gentle S inside the ACM box's own width */
  double BfmScanPeriodS() const override { return 30.0; }        /* ~1.7 deg/s of weave: a scan, not a turn */
  double BfmFloorFt() const override { return 2000.0; }

  /* INTERCEPT (systems/FBPilot's Intercept phase). Every number below comes out of THIS jet's two boxes
   * — the APG-68's geometry and the AIM-120's launch zone — rather than being chosen:
   *   SearchRadarModeOrdinal = CRM, the set's own power-up mode and the only F-16 mode that searches a
   *     large volume and does NOT auto-lock (modules/f16/FBF16Fcr's table). Its ordinal, not its name,
   *     because the generic layer is not allowed to know either.
   *   InterceptSpeedKt 550: knots TRUE, the unit FBAutopilot's speed loop regulates in
   *     (FBFlightControl differences it against fb_fdm_state::speed) — at the 8,000 m an intercept is
   *     flown at that is ~375 KCAS, i.e. this jet's measured 380 kt corner speed. Two things at once:
   *     the launch speed the round inherits, which is what Raero's integration starts from
   *     (modules/f16/FBF16FireControl::SolveLaunchZone), and a jet that is already at its best turn
   *     rate when the engagement stops being beyond visual range.
   *   InterceptLockRangeNm 16: outside every Rtr this weapon produces head-on (~11 nm at 8,000 m,
   *     measured — missions/intercept-aim120.fbm's launch record) and inside the APG-68's own 40 nm
   *     search gate, so the single-target track has settled and the fire control has published a launch
   *     zone before the shot range arrives, while the warning it hands the target is seconds rather
   *     than a minute.
   *   InterceptCrankAtaDeg 45: the STT gimbal envelope is +/-60 deg (FBF16Fcr::kGimbalAzDeg), and 15
   *     degrees of it is what a manoeuvring target needs before the track breaks. Cranking to the very
   *     limit is how a shot goes unsupported.
   *   InterceptShotAtaDeg 30: an AIM-120 leaves the rail pointed where the jet is pointed and has to
   *     pull onto the target; a boresight-launched round given more than about thirty degrees to make
   *     up spends its motor doing it.
   *   InterceptAbortRangeNm 5: inside this a BVR weapon's Rmin and the merge are the same problem, and
   *     doc/f16/ calls the fight from there a dogfight, not an intercept. */
  int    SearchRadarModeOrdinal() const override { return 1; }   /* FBF16FcrMode::Crm */
  double InterceptSpeedKt() const override { return 550.0; }
  double InterceptLockRangeNm() const override { return 16.0; }
  double InterceptShotRtrFactor() const override { return 1.0; }
  double InterceptShotAtaDeg() const override { return 30.0; }
  double InterceptShotSpacingS() const override { return 12.0; }
  double InterceptCrankAtaDeg() const override { return 45.0; }
  double InterceptAbortRangeNm() const override { return 5.0; }
  double InterceptBeamOffsetDeg() const override { return 90.0; }
  double InterceptChaffIntervalS() const override { return 3.0; }
  double InterceptDefendHoldS() const override { return 12.0; }

  /* ATTACK (systems/FBPilot's Attack phase) — this jet's air-to-ground delivery numbers. None of them
   * touches where the round goes; that is the fire control's, and the pilot only reads it.
   *   AttackReleaseBiasS 0: release ON the computed cue. It is a hook, not a setting — a mission that
   *     wants a wrong delivery declares one (`set pilot_attack_bias_s`), and the F-16 flies its passes
   *     on the cue.
   *   AttackCcipTolM 45: how far the predicted impact point may sit from the aim point and still be
   *     "the pipper is on it". [DERIVED from what the weapon does, not picked]: a Mk-82's own fragment
   *     pattern takes a soft installation out to ~25 m (modules/ground/FBGroundTarget.h) and degrades it
   *     to ~45 m, so a pass whose pipper is further off than that is a pass that would achieve nothing —
   *     which is exactly when a pilot does not pickle and comes round again.
   *   AttackEgressTurnDeg 135: a check turn well past the beam, so the jet is opening from its own
   *     detonation rather than flying along it. The F-16 holds this at the guidance's own bank limit.
   *   AttackEgressClimbM 600 / AttackEgressRangeM 12000 / AttackEgressS 30: the leg is a place in the
   *     world rather than a heading (systems/FBPilot's AttackCommands), so it needs a distance far
   *     enough ahead that the turn is a turn and not an arrival, and a hold long enough to complete it
   *     at this jet's turn rate — 135 deg at a 30 deg/s-limited roll and the standard bank limit is
   *     comfortably inside 30 s. */
  double AttackReleaseBiasS() const override { return 0.0; }
  double AttackCcipTolM() const override { return 45.0; }
  double AttackEgressTurnDeg() const override { return 135.0; }
  double AttackEgressClimbM() const override { return 600.0; }
  double AttackEgressRangeM() const override { return 12000.0; }
  double AttackEgressS() const override { return 30.0; }
};

} // namespace FlightBox
#endif
