/* FlightBox — FBBallistics: WHERE AN UNGUIDED STORE LANDS. The one arithmetic behind both air-to-ground
 * delivery modes (doc/f16/weapons.md §2.5's CCIP and CCRP), so the two cannot drift apart: they are the
 * same forward integration asked two different questions.
 *
 *   CCIP  "if I release now, where does it hit?"      -> FBSolveImpactPoint
 *   CCRP  "given that point down there, when do I release?" -> the same prediction, projected onto the
 *          current ground track against a designated point (FBSolveAim)
 *
 * WHAT IT INTEGRATES, and why it is deliberately NOT what the bomb then flies. The round becomes its own
 * JSBSim instance the moment it leaves the pylon (modules/stores/FBStoreModule), with the vendored
 * model's full aero — Mach-dependent drag, lift at whatever alpha it trims to, pitch damping. A real
 * fire-control computer has none of that: it carries a stored ballistic table (mass, one drag
 * coefficient, one reference area) and integrates a point mass. So does this, from core/FBStore.h's
 * FBWeaponPerf — the SAME table structure the guided rounds' launch zone already runs on, for the same
 * stated reason: the error between the computer's prediction and the flown result is a real property of
 * every delivery ever made, and feeding the prediction the weapon's own aerodynamics would hide it. The
 * CCIP/CCRP missions measure exactly that error.
 *
 * THE MODEL, in full, with every assumption on the surface:
 *   a = -g*u_up - (0.5*rho(h)*v^2*Cd*S/m) * v_hat
 * i.e. gravity plus axial drag along the (negative) velocity vector. Density is ISA at the CURRENT
 * altitude of the falling round (core/FBAtmosphere.h), re-evaluated every step — a bomb dropped from
 * 4 km falls through a third of the atmosphere on its way down, so a single-density approximation
 * (which is what the launch zone can afford over an engagement's flat band) would be wrong here by more
 * than the effect being measured. NOT modelled: lift (the round is treated as a point mass that never
 * develops alpha), wind (there is none in this simulator), the Coriolis term, and the Mach dependence of
 * Cd. Each of those is a stated omission of the COMPUTER, not of the simulation.
 *
 * THE IMPACT PLANE is handed in, never looked up: this file knows no terrain. The caller supplies the
 * elevation it is solving against, which for the F-16's fire control is the same elevation-provider
 * sample the radar altimeter and the mission's ground truth already use (modules/f16/FBF16FireControl).
 * A flat plane at that elevation is exactly what a jet with a barometric/steerpoint-elevation ranging
 * solution has ('B', the same provider letter the slant range already carries).
 *
 * HOT PATH: plain locals, fixed step count, no allocation. Deterministic — no time dependence beyond
 * the integration variable itself, no state, no randomness. */
#ifndef FBBALLISTICS_H
#define FBBALLISTICS_H

#include "FBStore.h"

namespace FlightBox {

/* The state a store is released in, as the fire control has it: the pylon's position and the velocity
 * vector it inherits from the carrier. ENU metres/second, geodetic degrees, m ASL. */
struct FBReleaseState {
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;
  double VelE = 0.0, VelN = 0.0, VelU = 0.0;
};

/* Where the integration says the round ends up. `Valid` is false for a table that cannot be integrated
 * (no mass, no reference area) or a release already at or below the impact plane. */
struct FBImpactPrediction {
  bool   Valid = false;
  double LatDeg = 0.0, LonDeg = 0.0;   /* the impact point — geodetic, hence double: a float carries
                                        * ~1e-5 deg, i.e. a metre, which is the quantity being measured */
  double ElevM = 0.0;                  /* the plane it was solved against (the caller's) */
  double TofS = 0.0;                   /* time of fall from release to that plane */
  double RangeM = 0.0;                 /* horizontal release point -> impact point */
  double BearingDeg = 0.0;             /* true, 0..360 */
  double ImpactSpeedMs = 0.0;          /* what it arrives with — the closure a ground burst resolves at */
  /* THE PULL-UP ANTICIPATION CUE (weapons.md §2.5: the altitude margin a release needs "for the fuze to
   * arm"), expressed as the margin the computation actually produces: how much of the fall is left over
   * once the store's arming delay has run. Negative = a release from here arrives unarmed, which is the
   * guide's dud case. The real jet draws this as a screen position moving toward the FPM; a margin in
   * seconds is the same fact in the form a decision is taken on, and it needs no second integration.
   * Zero delay (a store that declares none) leaves it equal to the whole time of fall. */
  double ArmMarginS = 0.0;
};

/* THE CCIP/CCRP INTEGRATION (see the file banner). `perf` is the FCC's stored table for the round —
 * only LaunchMassKg, DragCoefA, RefAreaM2 and ArmingS are read, because an unguided store has no motor
 * and no seeker. `impactElevM` is the plane to solve against, m ASL. */
FBImpactPrediction FBSolveImpactPoint(const FBWeaponPerf &perf, const FBReleaseState &rel,
                                      double impactElevM);

/* THE SAME PREDICTION, MEASURED AGAINST A TARGET — what turns an impact point into a release decision.
 * Both the predicted impact point and the designated point are projected onto the aircraft's CURRENT
 * ground track (core/FBGeodesy's FBTrackProjectM), which is the axis a release cue lives on: the round
 * can only be moved along it by waiting, and across it by turning.
 *
 * That one projection answers both delivery modes:
 *   CCIP reads MissM  — how far the pipper is from the target, the pilot's own "on target" test;
 *   CCRP reads AlongErrM/TimeToGoS — the Solution Cue's distance and countdown to the release point.
 * There is deliberately no second geometry for the second mode. */
struct FBAimSolution {
  bool   Valid = false;
  double AlongErrM = 0.0;   /* + = the round would fall SHORT of the target; 0 = release now */
  double CrossErrM = 0.0;   /* + = it would fall RIGHT of the target (the steering-line error) */
  double MissM = 0.0;       /* the two combined — the CCIP pipper's distance from the target */
  double TimeToGoS = 0.0;   /* AlongErrM at the current groundspeed; < 0 = the release point is past */
};

/* Invalid for an invalid prediction OR for an aircraft with no groundspeed: both errors are projections
 * onto the direction of motion, so without one there is no release point to be short of. */
FBAimSolution FBSolveAim(const FBImpactPrediction &pred, double ownLatDeg, double ownLonDeg,
                         double tgtLatDeg, double tgtLonDeg, double trackDeg, double groundSpeedMs);

} // namespace FlightBox
#endif
