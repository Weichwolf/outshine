#include "FBBallistics.h"
#include "FBAtmosphere.h"
#include "FBGeodesy.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox {

namespace {
constexpr double kG = 9.80665;
/* Cheap enough for the 10 Hz fire-control slot, fine enough that the step error is far below the
 * modelling error the prediction exists to expose (halving it moves the impact well under a metre). */
constexpr double kStepS = 0.05;
/* A leak guard, not physics: nothing this file integrates falls for two minutes. */
constexpr double kMaxTofS = 120.0;
} // namespace

FBImpactPrediction FBSolveImpactPoint(const FBWeaponPerf &perf, const FBReleaseState &rel,
                                      double impactElevM) {
  FBImpactPrediction p;
  p.ElevM = impactElevM;
  if (perf.LaunchMassKg <= 0.0 || perf.RefAreaM2 <= 0.0) return p;
  if (rel.AltM <= impactElevM) return p;

  /* Heun rather than Euler: the drag term is quadratic in speed, and Euler at this step biases the
   * range by an error of the same order as the effect being measured. */
  double e = 0.0, n = 0.0, u = 0.0;
  double ve = rel.VelE, vn = rel.VelN, vu = rel.VelU;
  double t = 0.0;
  const double k = 0.5 * perf.DragCoefA * perf.RefAreaM2 / perf.LaunchMassKg;   /* drag / (rho*v*v) */

  auto accel = [&](double aE, double aN, double aU, double altM, double &oE, double &oN, double &oU) {
    double v = std::sqrt(aE * aE + aN * aN + aU * aU);
    double d = k * FBIsaDensity(altM) * v;   /* |a_drag| / |v|, so the term below is linear in velocity */
    oE = -d * aE;
    oN = -d * aN;
    oU = -d * aU - kG;
  };

  double prevU = u, prevE = e, prevN = n, prevT = t;
  bool hit = false;
  while (t < kMaxTofS) {
    prevE = e; prevN = n; prevU = u; prevT = t;
    double a1E, a1N, a1U;
    accel(ve, vn, vu, rel.AltM + u, a1E, a1N, a1U);
    double pe = ve + a1E * kStepS, pn = vn + a1N * kStepS, pu = vu + a1U * kStepS;
    double a2E, a2N, a2U;
    accel(pe, pn, pu, rel.AltM + u + vu * kStepS, a2E, a2N, a2U);
    double he = 0.5 * (a1E + a2E), hn = 0.5 * (a1N + a2N), hu = 0.5 * (a1U + a2U);
    e += ve * kStepS + 0.5 * he * kStepS * kStepS;
    n += vn * kStepS + 0.5 * hn * kStepS * kStepS;
    u += vu * kStepS + 0.5 * hu * kStepS * kStepS;
    ve += he * kStepS; vn += hn * kStepS; vu += hu * kStepS;
    t += kStepS;
    if (rel.AltM + u <= impactElevM) { hit = true; break; }
  }
  if (!hit) return p;

  /* Interpolate inside the crossing step: quantising to 0.05 s throws ~15 m of range away. */
  double h0 = rel.AltM + prevU - impactElevM, h1 = rel.AltM + u - impactElevM;
  double f = (h0 - h1) > 1e-9 ? h0 / (h0 - h1) : 1.0;
  double ie = prevE + f * (e - prevE), in = prevN + f * (n - prevN);
  p.TofS = prevT + f * (t - prevT);
  p.ImpactSpeedMs = std::sqrt(ve * ve + vn * vn + vu * vu);
  p.ArmMarginS = p.TofS - perf.ArmingS;

  double coslat = std::cos(rel.LatDeg * kDeg2Rad);
  p.LatDeg = rel.LatDeg + in / kMPerDeg;
  p.LonDeg = rel.LonDeg + (coslat > 1e-6 ? ie / (kMPerDeg * coslat) : 0.0);
  p.RangeM = std::sqrt(ie * ie + in * in);
  p.BearingDeg = std::atan2(ie, in) * kRad2Deg;
  if (p.BearingDeg < 0.0) p.BearingDeg += 360.0;
  p.Valid = true;
  return p;
}

FBAimSolution FBSolveAim(const FBImpactPrediction &pred, double ownLatDeg, double ownLonDeg,
                         double tgtLatDeg, double tgtLonDeg, double trackDeg, double groundSpeedMs) {
  FBAimSolution a;
  /* A release cue needs a TRACK: both errors are projections onto the direction of motion, so a
   * stationary state yields no solution rather than a zero one. */
  if (!pred.Valid || groundSpeedMs <= 1.0) return a;
  double alongTgt = 0.0, acrossTgt = 0.0, alongImp = 0.0, acrossImp = 0.0;
  FBTrackProjectM(ownLatDeg, ownLonDeg, trackDeg, tgtLatDeg, tgtLonDeg, alongTgt, acrossTgt);
  FBTrackProjectM(ownLatDeg, ownLonDeg, trackDeg, pred.LatDeg, pred.LonDeg, alongImp, acrossImp);
  a.Valid = true;
  a.AlongErrM = alongTgt - alongImp;
  a.CrossErrM = acrossImp - acrossTgt;
  a.MissM = std::sqrt(a.AlongErrM * a.AlongErrM + a.CrossErrM * a.CrossErrM);
  a.TimeToGoS = groundSpeedMs > 1.0 ? a.AlongErrM / groundSpeedMs : 0.0;
  return a;
}

} // namespace FlightBox
