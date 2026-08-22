#include "Shear.h"

#include <cmath>

namespace outshine::Physics {

Shear ShedAt(const Slip &through, double loadN, double slipRad, double askedAlongN) {
  Shear out;
  if (!(loadN > 0.0)) { return out; }

  out.HoldN = through.Friction * loadN;
  out.AngleRad = slipRad;

  double across = through.StiffnessNPerRad * out.AngleRad;
  double along = askedAlongN;

  const double asked = std::sqrt(across * across + along * along);
  if (out.HoldN > 0.0 && asked > out.HoldN) {
    const double share = out.HoldN / asked;
    across *= share;
    along *= share;
    out.Sliding = true;
  }
  out.AcrossN = across;
  out.AlongN = along;
  out.Ratio = out.HoldN > 0.0 ? asked / out.HoldN : 0.0;
  return out;
}

Shear Shed(const Slip &through, double loadN, double acrossMs, double alongMs,
           double askedAlongN) {
  const double rollingMs = std::fabs(alongMs);
  const double slipRad = rollingMs > 0.0 ? std::atan2(-acrossMs, rollingMs) : 0.0;
  return ShedAt(through, loadN, slipRad, askedAlongN);
}

double Relaxed(const Slip &through, double wasRad, double isRad, double rolledM) {
  if (!(through.RelaxationM > 0.0) || !(rolledM > 0.0)) { return isRad; }
  const double caught = 1.0 - std::exp(-rolledM / through.RelaxationM);
  return wasRad + (isRad - wasRad) * caught;
}

} // namespace outshine::Physics
