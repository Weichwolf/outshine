#include "Rail.h"

#include <cmath>

namespace outshine::Pilot {

double OverturningMs2(const Rails &on, double gravityMs2) {
  if (!(on.CentreOfMassM > 0.0) || !(on.GaugeM > 0.0)) { return 0.0; }
  return gravityMs2 * 0.5 * on.GaugeM / on.CentreOfMassM;
}

Haul Ride(const Rails &on, const Envelope &within, const Demand &asked, const Where &at,
          double speedMs) {
  Haul out;
  if (!asked.Held) { return out; }

  out.UnbalancedMs2 = speedMs * speedMs * at.CurvaturePerM - within.GravityMs2 * std::sin(at.BankRad);
  out.PastCant = on.CantDeficiencyMs2 > 0.0 && std::fabs(out.UnbalancedMs2) > on.CantDeficiencyMs2;
  const double overturning = OverturningMs2(on, within.GravityMs2);
  out.Overturns = overturning > 0.0 && std::fabs(out.UnbalancedMs2) > overturning;

  if (asked.AlongMs2 >= 0.0) {
    const double reachable = within.AccelMs2();
    out.DriveN = within.MassKg * (asked.AlongMs2 < reachable ? asked.AlongMs2 : reachable);
  } else {
    const double reachable = within.BrakeMs2();
    out.BrakeN = within.MassKg * (-asked.AlongMs2 < reachable ? -asked.AlongMs2 : reachable);
  }
  return out;
}

}
