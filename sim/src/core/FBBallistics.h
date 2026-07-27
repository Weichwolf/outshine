/* WHERE AN UNGUIDED STORE LANDS — the one forward integration behind BOTH air-to-ground modes, asked
 * two questions (CCIP: where does it hit; CCRP: when do I release), so the two cannot drift apart.
 * It integrates the FCC's stored table (core/FBStore.h's FBWeaponPerf), deliberately NOT the aero the
 * bomb then flies: that prediction error is a real property of every delivery and the missions measure
 * it. a = -g*u_up - (0.5*rho(h)*v^2*Cd*S/m)*v_hat, ISA density re-evaluated every step; no lift, no
 * wind, no Coriolis, no Mach dependence of Cd — omissions of the COMPUTER, not of the simulation.
 * The impact PLANE is handed in, never looked up: this file knows no terrain.
 * doc/flightbox/core.md, Abschnitt 7.3. */
#ifndef FBBALLISTICS_H
#define FBBALLISTICS_H

#include "FBStore.h"

namespace FlightBox {

/* Pylon position + the velocity inherited from the carrier. ENU m/s, geodetic degrees, m ASL. */
struct FBReleaseState {
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;
  double VelE = 0.0, VelN = 0.0, VelU = 0.0;
};

/* `Valid` false: an unintegrable table (no mass/area) or a release already at/below the plane. */
struct FBImpactPrediction {
  bool   Valid = false;
  double LatDeg = 0.0, LonDeg = 0.0;   /* geodetic, hence double: a float carries ~1e-5 deg = a metre,
                                        * which is the quantity being measured */
  double ElevM = 0.0;                  /* the plane it was solved against (the caller's) */
  double TofS = 0.0;                   /* time of fall from release to that plane */
  double RangeM = 0.0;                 /* horizontal release point -> impact point */
  double BearingDeg = 0.0;             /* true, 0..360 */
  double ImpactSpeedMs = 0.0;          /* what it arrives with — the closure a ground burst resolves at */
  /* The pull-up anticipation cue (weapons.md §2.5) as the margin the computation actually produces:
   * fall left once the arming delay has run. Negative = it arrives unarmed. */
  double ArmMarginS = 0.0;
};

/* Only LaunchMassKg, DragCoefA, RefAreaM2 and ArmingS of `perf` are read — an unguided store has no
 * motor and no seeker. `impactElevM` is the plane to solve against, m ASL. */
FBImpactPrediction FBSolveImpactPoint(const FBWeaponPerf &perf, const FBReleaseState &rel,
                                      double impactElevM);

/* The same prediction MEASURED AGAINST A TARGET: both points projected onto the current ground track,
 * the axis a release cue lives on — the round moves along it only by waiting, across it only by
 * turning. One projection answers both modes; there is deliberately no second geometry. */
struct FBAimSolution {
  bool   Valid = false;
  double AlongErrM = 0.0;   /* + = the round would fall SHORT of the target; 0 = release now */
  double CrossErrM = 0.0;   /* + = it would fall RIGHT of the target (the steering-line error) */
  double MissM = 0.0;       /* the two combined — the CCIP pipper's distance from the target */
  double TimeToGoS = 0.0;   /* AlongErrM at the current groundspeed; < 0 = the release point is past */
};

/* Invalid without groundspeed too: both errors are projections onto the direction of motion, so
 * without one there is no release point to be short of — "no answer", not "release now". */
FBAimSolution FBSolveAim(const FBImpactPrediction &pred, double ownLatDeg, double ownLonDeg,
                         double tgtLatDeg, double tgtLonDeg, double trackDeg, double groundSpeedMs);

} // namespace FlightBox
#endif
