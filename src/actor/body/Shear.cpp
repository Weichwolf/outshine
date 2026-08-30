#include "Shear.h"

#include <cmath>

namespace outshine::Physics {

double FrictionAt(const Shearing &through, double loadN) {
  if (!(through.LoadFalloff > 0.0) || !(through.FrictionAtLoadN > 0.0) || !(loadN > 0.0)) {
    return through.Grip;
  }
  return through.Grip * std::pow(loadN / through.FrictionAtLoadN, -through.LoadFalloff);
}

double Brushed(double linearN, double holdN) {
  if (!(holdN > 0.0)) { return 0.0; }
  const double sign = linearN < 0.0 ? -1.0 : 1.0;
  const double reach = std::fabs(linearN) / (3.0 * holdN);
  if (reach >= 1.0) { return sign * holdN; }
  const double left = 1.0 - reach;
  return sign * holdN * (1.0 - left * left * left);
}

Shear ShedAt(const Shearing &through, double loadN, double slipRad, double askedAlongN) {
  Shear out;
  if (!(loadN > 0.0)) { return out; }

  out.HoldN = FrictionAt(through, loadN) * loadN;
  out.AngleRad = slipRad;

  double across = Brushed(through.CorneringNPerRad * out.AngleRad, out.HoldN);
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

Shear Shed(
    const Shearing &through, double loadN, double acrossMs, double alongMs, double askedAlongN) {
  const double rollingMs = std::fabs(alongMs);
  const double slipRad = rollingMs > 0.0 ? std::atan2(-acrossMs, rollingMs) : 0.0;
  return ShedAt(through, loadN, slipRad, askedAlongN);
}

double Relaxed(const Shearing &through, double wasRad, double isRad, double rolledM) {
  if (!(through.RelaxationM > 0.0) || !(rolledM > 0.0)) { return isRad; }
  const double caught = 1.0 - std::exp(-rolledM / through.RelaxationM);
  return wasRad + (isRad - wasRad) * caught;
}

}
