#include "Strut.h"

namespace outshine {

Touch Press(const Strut &strut, double clearanceM, double closingMs) {
  Touch out;
  out.ClosingMs = closingMs;
  const double reach = strut.RestLengthM + strut.WheelRadiusM;
  out.CompressionM = reach - clearanceM;
  if (!(out.CompressionM > 0.0)) {
    out.CompressionM = 0.0;
    return out;
  }
  out.OnGround = true;

  double within = out.CompressionM;
  double beyond = 0.0;
  if (strut.TravelM > 0.0 && within > strut.TravelM) {
    beyond = within - strut.TravelM;
    within = strut.TravelM;
    out.PastTravel = true;
  }
  out.SpringN = strut.SpringNPerM * within;
  out.BumpStopN = strut.BumpStopNPerM * beyond;
  out.DamperN = strut.DamperNsPerM * closingMs;

  out.LoadN = out.SpringN + out.BumpStopN + out.DamperN;
  if (out.LoadN < 0.0) { out.LoadN = 0.0; }
  out.PastLink = strut.LinkLimitN > 0.0 && out.LoadN > strut.LinkLimitN;
  return out;
}

double SagM(const Strut &strut, double loadN) {
  if (!(strut.SpringNPerM > 0.0) || !(loadN > 0.0)) { return 0.0; }
  const double onSpring = loadN / strut.SpringNPerM;
  if (strut.TravelM <= 0.0 || onSpring <= strut.TravelM) { return onSpring; }
  if (!(strut.BumpStopNPerM > 0.0)) { return strut.TravelM; }
  return strut.TravelM + (loadN - strut.SpringNPerM * strut.TravelM) / strut.BumpStopNPerM;
}

} // namespace outshine
