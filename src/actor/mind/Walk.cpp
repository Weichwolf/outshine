#include "Walk.h"

#include <algorithm>

namespace outshine::Pilot {

double TightestPerM(const Gait &of, double speedMs) {
  if (!(speedMs > 0.0) || !(of.TurnRateRadS > 0.0)) { return 0.0; }
  return of.TurnRateRadS / speedMs;
}

Stride Walk(const Gait &of, const Demand &asked, double speedMs) {
  Stride out;
  if (!asked.Held) { return out; }
  out.SpeedMs = asked.SpeedMs < of.TopMs ? asked.SpeedMs : of.TopMs;
  out.HeadingRateRadS = asked.CurvaturePerM * speedMs;
  out.HeadingRateRadS = std::min(out.HeadingRateRadS, of.TurnRateRadS);
  out.HeadingRateRadS = std::max(out.HeadingRateRadS, -of.TurnRateRadS);
  out.AlongMs2 = asked.AlongMs2;
  out.AlongMs2 = std::min(out.AlongMs2, of.AccelMs2);
  out.AlongMs2 = std::max(out.AlongMs2, -of.AccelMs2);
  return out;
}

} // namespace outshine::Pilot
