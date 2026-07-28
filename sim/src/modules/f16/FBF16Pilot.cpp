#include "FBF16Pilot.h"

namespace FlightBox::Modules {

/* doc/modules/f16/procedures-takeoff-taxi.md's weight/Vr table, piecewise-linear interpolated; clamped at the
 * table's ends (20,000..44,000 lb) rather than extrapolated. */
double FBF16Pilot::RotationSpeedKt(double grossWeightLbs) const {
  static const double kW[] = {20000, 24000, 28000, 32000, 36000, 40000, 44000};
  static const double kV[] = {  128,   142,   156,   168,   178,   188,   198};
  constexpr int n = sizeof(kW) / sizeof(kW[0]);
  if (grossWeightLbs <= kW[0]) return kV[0];
  if (grossWeightLbs >= kW[n - 1]) return kV[n - 1];
  for (int i = 1; i < n; i++) {
    if (grossWeightLbs <= kW[i]) {
      double t = (grossWeightLbs - kW[i - 1]) / (kW[i] - kW[i - 1]);
      return kV[i - 1] + t * (kV[i] - kV[i - 1]);
    }
  }
  return kV[n - 1];
}

} // namespace FlightBox::Modules
