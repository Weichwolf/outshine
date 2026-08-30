#include "Drive.h"

#include <cmath>

namespace outshine::Pilot {

double TightestPerM(const Axles &of, const Envelope &within, double speedMs) {
  if (!(of.WheelbaseM > 0.0)) { return 0.0; }
  const double byLock = std::tan(of.SteerLimitRad) / of.WheelbaseM;
  if (!(speedMs > 0.0)) { return byLock; }
  const double byGrip = within.LateralMs2() / (speedMs * speedMs);
  return byGrip < byLock ? byGrip : byLock;
}

Steering Drive(const Axles &of, const Envelope &within, const Demand &asked) {
  Steering out;
  if (!asked.Held) { return out; }
  out.SteerRad = std::atan(of.WheelbaseM * asked.CurvaturePerM);
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
