/* FlightBox — FBMig29Pilot: this jet's pilot NUMBERS. It overrides pilot/FBPilot's virtual hooks and
 * nothing else; the phase machine itself is generic, and that IS the stage-2a proof — a second airframe
 * flies the same Preflight->Takeoff->Climb->Route->Approach->Flare->Rollout procedure by declaring
 * different numbers, not different code.
 *
 * Provenance of every value below, per CLAUDE.md ("jede Zahl trägt ihre Herkunft"):
 *   [DOC]  stated in doc/mig29/procedures.md's quick-reference table, with its DCS-EA page
 *   [MESS] measured on the flown model by `make -C sim test-mig29` (anchor id given)
 *   [ABL]  derived, with the derivation written out here
 *   [SET]  a declared FlightBox setting, with its consequence named
 * The MiG-29's approach schedule is deliberately NOT a re-tuned F-16 one — doc/mig29/procedures.md §1
 * says so explicitly, and the numbers below are why: a faster final, a lower flare, a nozzle-strike
 * geometry 1.5 deg tighter than the flight monitor's contact-pitch limit, and a drag chute. */
#ifndef FBMIG29PILOT_H
#define FBMIG29PILOT_H

#include "FBPilot.h"

namespace FlightBox::Modules {

class FBMig29Pilot : public Pilot::FBPilot {
protected:
  /* [DOC] "nose raise 125...135 kts" — the midpoint, and [MESS] anchor B1 rotated at 130.1 kt when the
   * stage-1 harness commanded exactly this. NO WEIGHT TABLE: the F-16 has one because doc/f16 prints
   * one; nothing in doc/mig29/ makes Vr a function of weight, so none is invented. [GAP] */
  double RotationSpeedKt(double grossWeightLbs) const override { (void)grossWeightLbs; return 130.0; }
  /* ZERO, and that is a statement rather than an omission: the F-16's 15 kt lead exists because its
   * source quotes Vr as the speed at which the jet is already FLYING, whereas "nose raise 125...135" is
   * the speed at which the stick comes back. Leading it would rotate this aircraft 15 kt early. */
  double RotationLeadKt() const override { return 0.0; }
  double RotationPitchDeg() const override { return 9.0; }    /* [DOC] 8...10 deg on the HUD */
  /* [DOC] doc/mig29/procedures.md §1 note 5: the FC3 voice warning implies 250 kts gear-down. */
  double GearUpLimitKt() const override { return 250.0; }
  /* [DOC] "at 270 kts set RPM 83-85 %, hold until clear of the control zone". The phase ends when the
   * gear is up, so this is the climb-out schedule and not a cruise number. */
  double ClimbSpeedKt() const override { return 270.0; }
  double TakeoffThrottleNorm() const override { return 1.0; } /* [DOC] release into full augmentation */

  /* [ABL] The Approach phase holds ONE speed from glideslope capture to the flare, and the source gives
   * two: ">= 175 kts final" (a PATTERN speed, flown from base) and "touchdown ~140 kts at 11 deg AoA".
   * A single held speed cannot be 175 and still touch down at 140 through a 20-30 ft flare, so it is
   * placed between them: 150 kt is 9.6 % above the [MESS] anchor M1 (11 deg AoA sits at 136.8 KCAS,
   * landing config, 12 t), which puts the glidepath at ~9 deg AoA — inside the documented 15 deg
   * base-leg cap with room, and close enough that the flare has ~10 kt to bleed rather than ~35. */
  double ApproachSpeedKt() const override { return 150.0; }
  double GlidepathAngleDeg() const override { return 3.0; }   /* [SET] no source states one */
  /* ZERO because the DECK forbids it: fcs/speedbrake-permitted is 0 whenever gear-pos-norm > 0.05
   * ([DCS-EA p.57]: extension is inhibited with the gear down). Commanding 0.5 here would be a control
   * input with no effect, which reads in telemetry as a working speedbrake. */
  double ApproachSpeedbrakeNorm() const override { return 0.0; }
  /* [DOC] "flare initiation 20...30 ft AGL" — the TOP of the band, because the flare is also what bleeds
   * the ~10 kt between approach and touchdown speed and the extra 10 ft is the only time available. */
  double FlareStartAglFt() const override { return 30.0; }
  /* [DOC] touchdown at 11 deg AoA. Held as a PITCH attitude, which in the flattening flare is the same
   * angle; [DOC] "do not exceed 13 deg AoA" and the deck's own nozzle-strike geometry (13.5 deg of
   * pitch on the mains, mig29.xml <ground_reactions>) leave 2.5 deg of margin above it. */
  double FlareTargetPitchDeg() const override { return 11.0; }
  /* [ABL] 3.5 deg below the deck's 13.5 deg nozzle strike and 5 deg below core/FBFlightMonitor's 15 deg
   * contact-pitch KO. [DCS-EA p.78] names the nozzle strike as a real consequence of full aft stick, so
   * the aerobrake attitude is bounded by this aircraft's geometry and not by the F-16's 12 deg. */
  double AerobrakePitchDeg() const override { return 10.0; }
  double AerobrakeSpeedKt() const override { return 115.0; }  /* [DOC] brake application at 115 kts */
  double RolloutBrakeNorm() const override { return 0.8; }    /* [SET], the F-16's, and the drag chute
                                                               * deploys itself in the deck below 175 kt */

  /* BFM — [MESS] `make -C sim test-mig29`, anchors M2-VC/M2-G: the corner sweep (5,000 m, 13 t, 85 deg
   * of bank, the documented 26 deg / 9 g limits) peaks at 420 KCAS with 24.2 deg/s and a mean 7.83 g.
   * NOT REACHABLE THIS ROUND — stage 2a composes no radar, so pilot/FBBfmTrack never gets a contact and
   * the Bfm phase cannot be entered. They are here because they are measured, and because leaving the
   * generic 300 kt / 4 g placeholders would put an F-16-sized energy schedule on this airframe the
   * moment stage 2c switches the sensor on. */
  double BfmCornerSpeedKt() const override { return 420.0; }
  double BfmCornerG() const override { return 7.8; }
  double BfmMaxG() const override { return 9.0; }             /* [DOC] +9 g [DCS-FM p.14] */
  /* [MESS] the same sweep departed (alpha peak > 35 deg) at every entry from 300 to 360 kt and held at
   * 380 and above: below that the SOS stand-in cannot hold 85 deg of bank on this deck. */
  double BfmMinSpeedKt() const override { return 380.0; }
};

} // namespace FlightBox::Modules
#endif
