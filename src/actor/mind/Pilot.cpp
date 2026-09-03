#include "Pilot.h"
#include "Angle.h"

#include <algorithm>
#include <cmath>

namespace outshine::Pilot {

namespace {}

double ReachOf(const Holding &with, Travelling how) {
  double reachM = with.SettleS * (how.SpeedMs > 0.0 ? how.SpeedMs : 0.0);
  const double ramp = std::fabs(how.CurvatureRatePerM);
  if (with.HoldWithinM > 0.0 && ramp > 0.0) {
    const double byLag = std::cbrt(6.0 * with.HoldWithinM / ramp);
    reachM = std::min(byLag, reachM);
  }
  reachM = std::max(reachM, with.LeastReachM);
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

  const double reachM =
      ReachOf(with, {.SpeedMs = speedMs, .CurvatureRatePerM = at.CurvatureRatePerM});
  out.ReachM = reachM;

  const Sighting ahead = Sight(along, at, {.ChordM = reachM, .AsideM = with.AsideM});
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
