/* The gun's SHARED ballistic primitives — pure functions on values, so the fire control BEFORE the shot,
 * the projectiles AFTER it and the test harness use literally the same arithmetic. Sharing them is no
 * cheat here (unlike the missile's FBWeaponPerf): an unguided round IS the arc the FCC solves.
 * Point mass, gravity + quadratic drag, drag on the SPEED and gravity on the vertical separately — the
 * simplification that buys a closed form and an exact inverse t(s).
 * Derivation and error budget: doc/flightbox/core.md, Abschnitt 7.5. */
#ifndef FBGUNBALLISTICS_H
#define FBGUNBALLISTICS_H

#include "FBGun.h"

namespace FlightBox {

constexpr double kGravityMs2 = 9.80665;

/* The retardation constant k [1/m] of one round in air of density `rho`: 0.5*rho*Cd*A/m. */
double FBGunRetardation(const FBGunSpec &spec, double rho);

/* The closed forms of the drag-only trajectory. */
double FBGunSpeedAfter(double k, double v0, double t);   /* speed after t seconds */
double FBGunPathAfter(double k, double v0, double t);    /* path length after t seconds */
double FBGunTimeToPath(double k, double v0, double s);   /* ...and its exact inverse */

/* What one burst deposits on a target it passes [J/m^2] — the same currency as FBDamageModel's fragment
 * flux, so one register answers for both weapon effects. Circular-normal pattern of width `sigmaM`
 * against a disc of the presented area; the 1/range^2 falloff that makes a gun a short-range weapon is
 * DERIVED from it, not imposed. `impactSpeedMs` is relative, so the energy is the one the target sees.
 * Herleitung: doc/flightbox/core.md, Abschnitt 7.5. */
double FBGunFluxJm2(double rounds, const FBGunSpec &spec, double impactSpeedMs, double missM,
                    double sigmaM, double targetAreaM2, double extentM);

/* Expected rounds ON the target, for the record. TWO SCALES because an aircraft has two: `targetAreaM2`
 * is how much material, `extentM` how far out it reaches — the hits are the LARGER of the compact and
 * the extent reading, so a burst does not go from lethal to nothing over one metre of aim.
 * `extentM == 0` disables the extent reading. doc/flightbox/core.md, Abschnitt 7.5. */
double FBGunExpectedHits(double rounds, double missM, double sigmaM, double targetAreaM2,
                         double extentM);

/* THE LEAD SOLUTION — where the gun must point for rounds fired NOW to meet the target LATER: a
 * six-pass fixed point on s(t) = |rel + v_tgt*t + up*(g*t^2/2)|, then the closed-form BORE CANT that
 * cancels own crossing velocity (why a hard-turning fighter's rounds go where its nose is not, and the
 * physical origin of the EEGS funnel's shape). ENU, relative to the firing aircraft.
 * Herleitung: doc/flightbox/core.md, Abschnitt 7.5. */
struct FBGunAim {
  bool   Valid = false;                         /* false: target outruns the round, or own crossing
                                                 * velocity exceeds the muzzle velocity */
  double TofS = 0.0;                            /* projectile time of flight to the intercept point */
  double RangeM = 0.0;                          /* distance to that point */
  double BoreE = 0.0, BoreN = 0.0, BoreU = 0.0; /* unit vector the gun must point along */
  double SpreadM = 0.0;                         /* the pattern's sigma there (dispersion * path) */
  double ImpactSpeedMs = 0.0;                   /* round speed relative to the target at arrival */
};

FBGunAim FBGunSolveLead(const FBGunSpec &spec, double altM, double ownVelE, double ownVelN,
                        double ownVelU, double relE, double relN, double relU, double tgtVelE,
                        double tgtVelN, double tgtVelU);

} // namespace FlightBox
#endif
