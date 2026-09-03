#include "Fly.h"

#include <algorithm>
#include <cmath>

namespace outshine::Pilot {

double BankLimitOf(const Wings &of) {
  double limitRad = of.BankLimitRad;
  if (of.LoadFactorLimit > 1.0) {
    const double byLoad = std::acos(1.0 / of.LoadFactorLimit);
    limitRad = std::min(byLoad, limitRad);
  }
  return limitRad;
}

double TightestPerM(const Wings &of, double speedMs, double gravityMs2) {
  if (!(speedMs > 0.0)) { return 0.0; }
  return gravityMs2 * std::tan(BankLimitOf(of)) / (speedMs * speedMs);
}

Flying Fly(const Wings &of, const Envelope &within, const Demand &asked) {
  Flying out;
  if (!asked.Held) { return out; }

  const double speedMs = asked.SpeedMs;
  out.BankRad = std::atan(speedMs * speedMs * asked.CurvaturePerM / within.GravityMs2);
  const double limitRad = BankLimitOf(of);
  out.BankRad = std::min(out.BankRad, limitRad);
  out.BankRad = std::max(out.BankRad, -limitRad);
  out.LoadFactor = 1.0 / std::cos(out.BankRad);

  out.ClimbRad = asked.ClimbRad;
  out.ClimbRad = std::min(out.ClimbRad, of.ClimbLimitRad);
  out.ClimbRad = std::max(out.ClimbRad, -of.ClimbLimitRad);

  const double dragN = 0.5 * within.AirDensity * within.DragArea * speedMs * speedMs;
  const double weightN = within.MassKg * within.GravityMs2;
  out.ThrustN = dragN + weightN * std::sin(out.ClimbRad) + within.MassKg * asked.AlongMs2;
  out.ThrustN = std::max(out.ThrustN, 0.0);
  if (within.DriveN > 0.0 && out.ThrustN > within.DriveN) { out.ThrustN = within.DriveN; }
  return out;
}

} // namespace outshine::Pilot
