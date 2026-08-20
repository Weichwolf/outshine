#include "Pilot.h"

#include <cmath>

namespace outshine::Pilot {

namespace {

constexpr double kTurn = 6.283185307179586;

double Wrapped(double angleRad) {
  while (angleRad > 0.5 * kTurn) { angleRad -= kTurn; }
  while (angleRad < -0.5 * kTurn) { angleRad += kTurn; }
  return angleRad;
}

} // namespace

double ReachOf(const Reins &with, double speedMs) {
  double reachM = with.SettleS * (speedMs > 0.0 ? speedMs : 0.0);
  if (reachM < with.LeastReachM) { reachM = with.LeastReachM; }
  return reachM;
}

Demand Hold(const ReferenceLine &along, const Reins &with, const Placement &at, double speedMs,
            double wantedMs) {
  Demand out;
  out.SpeedMs = wantedMs;
  if (!at.Found || !(with.SettleS > 0.0)) { return out; }

  const double reachM = ReachOf(with, speedMs);
  out.ReachM = reachM;

  const Sighting ahead = Sight(along, at, reachM);
  if (!ahead.Found) { return out; }
  out.AtEnd = ahead.AtEnd;
  out.OutOfReach = ahead.OutOfReach;

  const double offAxisRad = Wrapped(ahead.BearingRad - at.HeadingRad);
  out.AskedPerM = 2.0 * std::sin(offAxisRad) / ahead.ChordM;
  out.CurvaturePerM = out.AskedPerM;
  if (with.TightestPerM > 0.0) {
    if (out.CurvaturePerM > with.TightestPerM) {
      out.CurvaturePerM = with.TightestPerM;
      out.Saturated = true;
    } else if (out.CurvaturePerM < -with.TightestPerM) {
      out.CurvaturePerM = -with.TightestPerM;
      out.Saturated = true;
    }
  }

  out.ClimbRad = ahead.ClimbRad;
  out.AlongMs2 = (wantedMs - speedMs) / with.SettleS;
  out.Held = true;
  return out;
}

} // namespace outshine::Pilot
