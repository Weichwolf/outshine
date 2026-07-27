/* FlightBox — the gun's SHARED BALLISTIC PRIMITIVES: the trajectory of a fired round, the lead solution
 * that puts it where the target will be, and the energy a burst deposits when it gets there. Pure
 * functions on values, no state, no allocation — which is what lets the three consumers that must agree
 * use literally the same arithmetic instead of three copies of it:
 *   modules/f16/FBF16FireControl  computes the EEGS aiming solution BEFORE the shot,
 *   core/FBGunProjectiles         flies the rounds AFTER it, and
 *   app/FBTestGun                 checks both against doc/f16/weapons.md's own numbers.
 * A fire-control computer whose prediction is the same code as the flown trajectory would normally be a
 * cheat (it is why core/FBStore.h's FBWeaponPerf is deliberately a coarse SEPARATE copy of the missile's
 * aerodynamics). It is not one here, and the difference is worth stating: an AMRAAM's flight is a guided
 * airframe's — its own JSBSim model, its own autopilot, its own energy management — and no table can
 * predict it exactly. A 20 mm round is an unguided lump on a ballistic arc, and the F-16's FCC solves
 * precisely that arc. Modelling a disagreement between the two would be inventing an error, not
 * measuring one.
 *
 * THE TRAJECTORY MODEL (physics, with its one simplification named): a point mass under gravity and
 * quadratic drag, dv/dt = -k*v^2 along the velocity, k = 0.5*rho*Cd*A/m against the ISA density at the
 * firing altitude (core/FBAtmosphere.h). The SIMPLIFICATION is that drag is applied to the SPEED and
 * gravity to the velocity's vertical component separately, rather than to the vector sum: over a round's
 * whole useful life (under 2 s, under 2 km) the drop is metres against a path of kilometres, so the
 * angle between the two is small and the decoupling is worth a few centimetres. What it buys is a
 * CLOSED FORM — v(t) = v0/(1+k*v0*t), s(t) = ln(1+k*v0*t)/k — and therefore an exact, iteration-free
 * inverse t(s), which is what makes the lead solve below converge in a handful of fixed steps with no
 * search and no per-frame allocation. */
#ifndef FBGUNBALLISTICS_H
#define FBGUNBALLISTICS_H

#include "FBGun.h"

namespace FlightBox {

constexpr double kGravityMs2 = 9.80665;

/* The retardation constant k [1/m] of one round in air of density `rho`: 0.5*rho*Cd*A/m. */
double FBGunRetardation(const FBGunSpec &spec, double rho);

/* The closed forms of the drag-only trajectory (see the file banner). */
double FBGunSpeedAfter(double k, double v0, double t);   /* speed after t seconds */
double FBGunPathAfter(double k, double v0, double t);    /* path length after t seconds */
double FBGunTimeToPath(double k, double v0, double s);   /* ...and its exact inverse */

/* WHAT ONE BURST DEPOSITS ON A TARGET IT PASSES, in J/m^2 of areal energy — the same currency
 * core/FBDamageModel's fragment flux is expressed in, so one damage register can answer for both weapon
 * effects without a second set of thresholds.
 *
 * The model, in one line: the rounds are a circular-normal pattern of width `sigma` about the bundle's
 * axis, the TARGET is a disc of the same area it presents, and the expected number of rounds on it is
 * the overlap of the two. Writing the target disc as its own equivalent normal (sigma_t^2 = A/(2*pi),
 * the width whose central density matches a disc of area A) makes that overlap a closed form:
 *     hits = N * A/(A + 2*pi*sigma^2) * exp(-d^2 / (2*(sigma^2 + A/(2*pi))))
 * with d the miss distance from the target's centre. Each round carries E = 0.5*m*v_rel^2 of kinetic
 * energy in the target's own frame, and the flux is those hits spread over whichever is smaller, the
 * pattern or the target. Every limit falls out of the one formula —
 *   pattern much larger than the target: the target intercepts N*A/(2*pi*sigma^2) rounds and the flux
 *     falls as 1/range^2, because sigma grows linearly with range. That is the reason a gun is a
 *     short-range weapon, and it is derived here rather than imposed by a range limit;
 *   pattern much smaller than the target: every round hits, in a spot of area 2*pi*sigma^2, and the flux
 *     saturates at what a point-blank burst does;
 *   the burst passing a few metres off: the target's own EXTENT still catches part of it, which is the
 *     term a point-target model gets wrong — an aircraft is metres across, and a pattern centred on its
 *     wingtip still puts rounds into the wing.
 * `sigmaM` is the pattern's linear sigma at the target (spec.DispersionSigmaRad * path length),
 * `missM` the distance from the bundle's axis to the target's centre at closest approach, and
 * `impactSpeedMs` the RELATIVE speed of round and target — the energy is the one the target sees, so a
 * head-on burst arrives harder than a stern-chase one and nothing here has to say so separately. */
double FBGunFluxJm2(double rounds, const FBGunSpec &spec, double impactSpeedMs, double missM,
                    double sigmaM, double targetAreaM2, double extentM);

/* Expected number of rounds ON the target, for the record (the flux above does not need it, but a hit
 * report that says "0.4 rounds" is more honest than one that says "a hit"). Same density model.
 *
 * TWO SCALES, because an aircraft has two. `targetAreaM2` is how much MATERIAL it presents; `extentM`
 * is how far that material reaches from its centre (half a span seen from astern, half a length seen
 * from the side). A single disc of the presented area cannot express both: it is right for a burst on
 * the fuselage and says "nothing at all" for one four metres out, where a real F-16 still has wing. So
 * the expected hits are the LARGER of two readings of the same pattern —
 *   COMPACT: the material as one disc of area A. Exact when the burst is on the centre, which is where
 *            a killing burst is;
 *   EXTENT:  the material spread thinly over the whole silhouette disc of radius `extentM`, so a round
 *            landing inside it hits something with probability A/(pi*extent^2). Right for the wing.
 * Neither is a measurement and both are stated: what the pair buys is that a burst does not go from
 * lethal to literally nothing over one metre of aim. An `extentM` of zero disables the second reading
 * entirely (a target that has declared no extent is treated as compact). */
double FBGunExpectedHits(double rounds, double missM, double sigmaM, double targetAreaM2,
                         double extentM);

/* THE LEAD SOLUTION — where the gun has to point for rounds fired NOW to meet the target LATER.
 *
 * It is a fixed-point solve of one equation: the round's path length s(t) has to equal the distance to
 * where the target will be at t, in a frame where the round starts at the aircraft. Written out, the
 * displacement the round has to cover is
 *     D(t) = rel + v_target*t + up*(0.5*g*t^2)
 * — the target's current offset, its motion during the flight, and the drop the round must be aimed
 * ABOVE to compensate. Given a direction, t follows exactly from FBGunTimeToPath; given t, the direction
 * follows from D(t). Six passes settle it to well under a metre at any range the gun is used at.
 *
 * The last step is the one a naive lead computation gets wrong: the round leaves with the AIRCRAFT'S
 * velocity added to its muzzle velocity, so the direction the round travels is NOT the direction the
 * barrel points. Solving that is closed-form — decompose the aircraft's velocity into the components
 * along and across the required flight direction, and the bore has to be canted across by exactly enough
 * for the muzzle velocity to cancel the crossing component:
 *     bore = (mu*flightdir - v_own_across) / |...|,  mu = sqrt(v_muzzle^2 - |v_own_across|^2)
 * That cant is why a hard-turning fighter's rounds go where its nose is not, and it is the physical
 * origin of the EEGS funnel's shape.
 *
 * Everything is ENU metres/metres-per-second relative to the firing aircraft's own position. Valid=false
 * means there is no solution to be had: the target is running faster than the round can close, or the
 * aircraft is crossing so fast that the muzzle velocity cannot cancel it (which cannot happen with a
 * 1,030 m/s muzzle and an aircraft, and is checked rather than assumed). */
struct FBGunAim {
  bool   Valid = false;
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
