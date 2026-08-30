#include "Pilot.h"
#include "Angle.h"

#include <cmath>

namespace outshine::Pilot {

namespace {}

double ReachOf(const Holding &with, double speedMs, double curvatureRatePerM) {
  double reachM = with.SettleS * (speedMs > 0.0 ? speedMs : 0.0);
  const double ramp = std::fabs(curvatureRatePerM);
  if (with.HoldWithinM > 0.0 && ramp > 0.0) {
    const double byLag = std::cbrt(6.0 * with.HoldWithinM / ramp);
    if (byLag < reachM) { reachM = byLag; }
  }
  if (reachM < with.LeastReachM) { reachM = with.LeastReachM; }
  return reachM;
}

Demand Hold(const ReferenceLine &along,
            const Holding &with,
            const Where &at,
            double speedMs,
            double wantedMs) {
  Demand out;
  out.SpeedMs = wantedMs;
  if (!at.Found || !(with.SettleS > 0.0)) { return out; }

  const double reachM = ReachOf(with, speedMs, at.CurvatureRatePerM);
  out.ReachM = reachM;

  const Sighting ahead = Sight(along, at, reachM, with.AsideM);
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
