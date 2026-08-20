#include "Carriageway.h"

#include <cmath>

namespace outshine {

Standing Stand(const ReferenceLine &over, double eastM, double northM, double halfWidthM,
               double nearM, double windowM) {
  Standing out;
  double alongM = 0.0;
  if (!over.Nearest(eastM, northM, nearM, windowM, alongM)) { return out; }

  Placed on;
  if (!over.At(alongM, on)) { return out; }

  const double left[2] = {-std::sin(on.HeadingRad), std::cos(on.HeadingRad)};
  out.AlongM = alongM;
  out.AcrossM = (eastM - on.EastM) * left[0] + (northM - on.NorthM) * left[1];
  out.On = halfWidthM <= 0.0 || std::fabs(out.AcrossM) <= halfWidthM;

  const double bank = std::tan(on.BankRad);
  out.HeightM = on.HeightM - out.AcrossM * bank;

  const double ahead[3] = {std::cos(on.HeadingRad), on.Slope, std::sin(on.HeadingRad)};
  const double across[3] = {left[0], -bank, left[1]};
  double normal[3] = {ahead[1] * across[2] - ahead[2] * across[1],
                      ahead[2] * across[0] - ahead[0] * across[2],
                      ahead[0] * across[1] - ahead[1] * across[0]};
  const double length =
      std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
  if (length > 0.0) {
    const double sign = normal[1] < 0.0 ? -1.0 : 1.0;
    for (int axis = 0; axis < 3; ++axis) { out.NormalM[axis] = sign * normal[axis] / length; }
  }
  return out;
}

} // namespace outshine
