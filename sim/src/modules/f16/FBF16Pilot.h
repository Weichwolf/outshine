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
 * closed AoA loop that is faithful to the model's own trim curve rather than a copied real-jet number. */
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
};

} // namespace FlightBox
#endif
