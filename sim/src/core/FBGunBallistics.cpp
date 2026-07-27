#include "FBGunBallistics.h"
#include "FBAtmosphere.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

double FBGunRetardation(const FBGunSpec &spec, double rho) {
  if (spec.RoundMassKg <= 0.0 || spec.RoundDiaM <= 0.0) return 0.0;
  double area = 0.25 * kPi * spec.RoundDiaM * spec.RoundDiaM;
  return 0.5 * rho * spec.DragCoef * area / spec.RoundMassKg;
}

double FBGunSpeedAfter(double k, double v0, double t) {
  if (k <= 0.0) return v0;
  return v0 / (1.0 + k * v0 * t);
}

double FBGunPathAfter(double k, double v0, double t) {
  if (k <= 0.0) return v0 * t;
  return std::log(1.0 + k * v0 * t) / k;
}

double FBGunTimeToPath(double k, double v0, double s) {
  if (v0 <= 0.0) return 0.0;
  if (k <= 0.0) return s / v0;
  /* exp(k*s) overflows long before any range a gun is used at — the guard keeps a nonsense input from
   * propagating an infinity into a pose. */
  double e = k * s;
  if (e > 20.0) return -1.0;
  return (std::exp(e) - 1.0) / (k * v0);
}

namespace {
/* Fraction of a circular-normal pattern that lands inside a disc written as its equivalent normal. */
double PatternOverlap(double missM, double sigmaM, double sigmaT2) {
  double s2 = sigmaM * sigmaM + sigmaT2;
  return (sigmaT2 / s2) * std::exp(-(missM * missM) / (2.0 * s2));
}
} // namespace

double FBGunExpectedHits(double rounds, double missM, double sigmaM, double targetAreaM2,
                         double extentM) {
  if (rounds <= 0.0 || sigmaM <= 0.0 || targetAreaM2 <= 0.0) return 0.0;
  /* COMPACT: all the material in one disc of area A (sigma_t^2 = A/(2*pi) is the equivalent normal). */
  double compact = PatternOverlap(missM, sigmaM, targetAreaM2 / (2.0 * kPi));
  double frac = compact;
  /* EXTENT: the same material spread over the silhouette, hit with probability A/(pi*extent^2). */
  if (extentM > 0.0) {
    double silhouette = kPi * extentM * extentM;
    if (silhouette > targetAreaM2) {
      double fill = targetAreaM2 / silhouette;
      double spread = fill * PatternOverlap(missM, sigmaM, extentM * extentM * 0.5);
      if (spread > frac) frac = spread;
    }
  }
  double hits = rounds * frac;
  return hits > rounds ? rounds : hits;   /* a bundle cannot land more rounds than it holds */
}

double FBGunFluxJm2(double rounds, const FBGunSpec &spec, double impactSpeedMs, double missM,
                    double sigmaM, double targetAreaM2, double extentM) {
  if (rounds <= 0.0 || sigmaM <= 0.0 || targetAreaM2 <= 0.0 || impactSpeedMs <= 0.0) return 0.0;
  double energyPerRound = 0.5 * spec.RoundMassKg * impactSpeedMs * impactSpeedMs;
  double hits = FBGunExpectedHits(rounds, missM, sigmaM, targetAreaM2, extentM);
  /* Over WHICH area: the pattern's, or the target's when the pattern is smaller than it. */
  double patternM2 = 2.0 * kPi * sigmaM * sigmaM;
  double areaM2 = patternM2 < targetAreaM2 ? patternM2 : targetAreaM2;
  return hits * energyPerRound / areaM2;
}

FBGunAim FBGunSolveLead(const FBGunSpec &spec, double altM, double ownVelE, double ownVelN,
                        double ownVelU, double relE, double relN, double relU, double tgtVelE,
                        double tgtVelN, double tgtVelU) {
  FBGunAim a;
  double range0 = std::sqrt(relE * relE + relN * relN + relU * relU);
  if (range0 < 1.0 || spec.MuzzleVelMs <= 0.0) return a;

  const double k = FBGunRetardation(spec, FBIsaDensity(altM));
  /* Seed: the direction to the target now, and the flight time for a round leaving at muzzle speed. */
  double de = relE / range0, dn = relN / range0, du = relU / range0;
  double t = FBGunTimeToPath(k, spec.MuzzleVelMs, range0);
  if (t < 0.0) return a;

  double dist = range0, mu = spec.MuzzleVelMs, v0 = spec.MuzzleVelMs;
  for (int i = 0; i < 6; i++) {
    /* Where the round must go, including the drop it has to be aimed above. */
    double De = relE + tgtVelE * t;
    double Dn = relN + tgtVelN * t;
    double Du = relU + tgtVelU * t + 0.5 * kGravityMs2 * t * t;
    dist = std::sqrt(De * De + Dn * Dn + Du * Du);
    if (dist < 1.0) return a;
    de = De / dist; dn = Dn / dist; du = Du / dist;

    /* Ground-frame launch speed along that direction: own velocity along it, plus whatever muzzle
     * velocity is left after cancelling the crossing component. */
    double along = ownVelE * de + ownVelN * dn + ownVelU * du;
    double ce = ownVelE - along * de, cn = ownVelN - along * dn, cu = ownVelU - along * du;
    double cross2 = ce * ce + cn * cn + cu * cu;
    if (cross2 >= spec.MuzzleVelMs * spec.MuzzleVelMs) return a;   /* crossing faster than the muzzle */
    mu = std::sqrt(spec.MuzzleVelMs * spec.MuzzleVelMs - cross2);
    v0 = along + mu;
    if (v0 <= 1.0) return a;                                       /* the round never gets there */
    double tn = FBGunTimeToPath(k, v0, dist);
    if (tn < 0.0) return a;
    t = tn;
  }

  /* The bore: cancel the crossing component, the rest along the flight direction. */
  double along = ownVelE * de + ownVelN * dn + ownVelU * du;
  double be = (mu * de - (ownVelE - along * de)) / spec.MuzzleVelMs;
  double bn = (mu * dn - (ownVelN - along * dn)) / spec.MuzzleVelMs;
  double bu = (mu * du - (ownVelU - along * du)) / spec.MuzzleVelMs;
  double bl = std::sqrt(be * be + bn * bn + bu * bu);
  if (bl < 1e-9) return a;

  double vAtTarget = FBGunSpeedAfter(k, v0, t);
  /* What the TARGET sees — the closing speed at impact is what carries the energy. */
  double tgtAlong = tgtVelE * de + tgtVelN * dn + tgtVelU * du;

  a.Valid = true;
  a.TofS = t;
  a.RangeM = dist;
  a.BoreE = be / bl; a.BoreN = bn / bl; a.BoreU = bu / bl;
  a.SpreadM = spec.DispersionSigmaRad * dist;
  a.ImpactSpeedMs = vAtTarget - tgtAlong;
  if (a.ImpactSpeedMs < 0.0) a.ImpactSpeedMs = 0.0;
  return a;
}

} // namespace FlightBox
