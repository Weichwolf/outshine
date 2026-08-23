#include "Walk.h"

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
  if (out.HeadingRateRadS > of.TurnRateRadS) { out.HeadingRateRadS = of.TurnRateRadS; }
  if (out.HeadingRateRadS < -of.TurnRateRadS) { out.HeadingRateRadS = -of.TurnRateRadS; }
  out.AlongMs2 = asked.AlongMs2;
  if (out.AlongMs2 > of.AccelMs2) { out.AlongMs2 = of.AccelMs2; }
  if (out.AlongMs2 < -of.AccelMs2) { out.AlongMs2 = -of.AccelMs2; }
  return out;
}

}
