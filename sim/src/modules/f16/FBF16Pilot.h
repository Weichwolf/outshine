/* FlightBox — FBF16Pilot: this jet's pilot NUMBERS. It overrides systems/FBPilot's virtual hooks and
 * nothing else — the phase machine itself is generic. Every value below, its provenance marker
 * ([DOC]/[MESS]/[ABL]/[SET]) and the two accepted vanilla-model properties that shape the BFM figures:
 * doc/modules-f16.md §3. */
#ifndef FBF16PILOT_H
#define FBF16PILOT_H

#include "FBPilot.h"

namespace FlightBox::Modules {

class FBF16Pilot : public Pilot::FBPilot {
protected:
  double RotationSpeedKt(double grossWeightLbs) const override;
  double RotationLeadKt() const override { return 15.0; }
  double RotationPitchDeg() const override { return 10.0; }
  double GearUpLimitKt() const override { return 300.0; }
  double ClimbSpeedKt() const override { return 350.0; }
  double TakeoffThrottleNorm() const override { return 1.0; }

  /* [MESS] trimmed level, gear down, 40 % fuel: 11.0 deg AoA sits at 154 KCAS on this model (165 KCAS
   * before assets/MODEL-DELTAS.md D1 made the trailing edge flaps carry). */
  double ApproachSpeedKt() const override { return 154.0; }
  double GlidepathAngleDeg() const override { return 3.0; }
  double ApproachSpeedbrakeNorm() const override { return 0.5; }
  double FlareStartAglFt() const override { return 50.0; }
  double FlareTargetPitchDeg() const override { return 12.5; }
  double AerobrakePitchDeg() const override { return 12.0; }
  double AerobrakeSpeedKt() const override { return 100.0; }
  double RolloutBrakeNorm() const override { return 0.8; }

  /* Corner speed and its g are [MESS] against the flown model (`make -C sim test-corner`); re-measured
   * after assets/MODEL-DELTAS.md D1 — the speed is unchanged, the g at it fell from 5.6 to 5.4 because
   * a roll-in no longer comes with a spurious lift jump. */
  double BfmCornerSpeedKt() const override { return 380.0; }
  double BfmCornerG() const override { return 5.4; }
  double BfmMaxG() const override { return 9.0; }
  double BfmMinSpeedKt() const override { return 300.0; }
  double BfmUnloadG() const override { return 3.0; }
  double BfmControlMinNm() const override { return 0.5; }
  double BfmControlMaxNm() const override { return 1.5; }
  double BfmControlAspectDeg() const override { return 30.0; }
  double BfmControlAtaDeg() const override { return 30.0; }
  double BfmClosureGainKtPerNm() const override { return 120.0; }
  double BfmMaxClosureKt() const override { return 200.0; }
  /* [MESS] the decay of the CLOSURE under idle + speedbrake in the stern conversion (N=4595 one-second
   * windows over the 16-approach gun sweep): median 1.86, p20 1.16, p90 5.76 m/s^2. Not the airframe's
   * level-flight deceleration (2.4-2.53) that stood here before: the schedule bounds a closure, and a
   * closure carries the geometry as well as the drag. A braking LIMIT takes the pessimistic end of its
   * own distribution — a cap the pursuer meets half the time is one he breaks half the time.
   * doc/pilot.md 5.2. */
  double BfmBrakeMs2() const override { return 1.2; }
  double BfmLeadAspectDeg() const override { return 45.0; }
  double BfmLeadRangeNm() const override { return 3.0; }
  double BfmLeadMaxS() const override { return 4.0; }
  double BfmLagTimeS() const override { return 2.5; }
  double BfmYoYoHeightM() const override { return 600.0; }
  double BfmScanAfterS() const override { return 3.0; }
  double BfmScanAmplitudeDeg() const override { return 8.0; }    /* a gentle S inside the ACM box's own width */
  double BfmScanPeriodS() const override { return 30.0; }        /* ~1.7 deg/s of weave: a scan, not a turn */
  double BfmFloorFt() const override { return 2000.0; }

  /* INTERCEPT — every number [ABL] from the APG-68's geometry or the AIM-120's launch zone rather than
   * chosen; the derivations are doc/modules-f16.md §3.4. */
  int    SearchRadarModeOrdinal() const override { return 1; }   /* FBF16FcrMode::Crm, as an ORDINAL:
                                                                  * the generic layer knows neither */
  /* EMCON (doc/duels.md D3c). This jet has no separate emission switch — its module REJECTS
   * `RadarEmission` as NotImplemented, and rightly so — but `FBF16FcrMode::Off` sets `Active = false`,
   * so the FCR knob IS the emission control and nothing new is claimed. The radiate range is the
   * FCR's own search gate against a fighter-size target, not a number chosen here. */
  int    SilentRadarModeOrdinal() const override { return 0; }   /* FBF16FcrMode::Off */
  double EmconRadiateNm() const override { return 40.0; }        /* [= FBF16Fcr::kSearchRangeNm] */
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

  /* ATTACK — none of these touches where the round goes; that is the fire control's, and the pilot only
   * reads it. Derivations: doc/modules-f16.md §3.5. */
  double AttackReleaseBiasS() const override { return 0.0; }
  double AttackCcipTolM() const override { return 45.0; }
  double AttackEgressTurnDeg() const override { return 135.0; }
  double AttackEgressClimbM() const override { return 600.0; }
  double AttackEgressRangeM() const override { return 12000.0; }
  double AttackEgressS() const override { return 30.0; }
};

} // namespace FlightBox::Modules
#endif
