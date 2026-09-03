#include <optional>
#include "math/Vec2.h"
#include "Carriageway.h"
#include "math/Vec3.h"

#include <cmath>

namespace outshine {

namespace {

Astride Surface(const Placed &on, Astraddle at) {
  const double alongM = at.AlongM;
  const double acrossM = at.AcrossM;
  const double halfWidthM = at.HalfWidthM;
  Astride out;
  out.AlongM = alongM;
  out.AcrossM = acrossM;
  out.On = halfWidthM <= 0.0 || std::fabs(acrossM) <= halfWidthM;

  const Vec2 left = {{-std::sin(on.HeadingRad), std::cos(on.HeadingRad)}};
  const double bank = std::tan(on.BankRad);
  out.HeightM = on.HeightM - out.AcrossM * bank;

  const Vec3 ahead = {{std::cos(on.HeadingRad), on.Slope, std::sin(on.HeadingRad)}};
  const Vec3 across = {{left[0], -bank, left[1]}};
  const Vec3 normal = {{ahead[1] * across[2] - ahead[2] * across[1],
                        ahead[2] * across[0] - ahead[0] * across[2],
                        ahead[0] * across[1] - ahead[1] * across[0]}};
  const double length =
      std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
  if (length > 0.0) {
    const double sign = normal[1] < 0.0 ? -1.0 : 1.0;
    for (int axis = 0; axis < 3; ++axis) { out.NormalM[axis] = sign * normal[axis] / length; }
  }
  return out;
}

} // namespace

Astride Stand(const ReferenceLine &over, EastNorth at, double halfWidthM, Nearby about) {
  const std::optional<double> found = over.Nearest(at, about);
  if (!found) { return {}; }
  const double alongM = *found;
  Placed on;
  if (!over.At(alongM, on)) { return {}; }
  const Vec2 left = {{-std::sin(on.HeadingRad), std::cos(on.HeadingRad)}};
  const double acrossM = (at.EastM - on.EastM) * left[0] + (at.NorthM - on.NorthM) * left[1];
  return Surface(on, {.AlongM = alongM, .AcrossM = acrossM, .HalfWidthM = halfWidthM});
}

Astride StandAt(const ReferenceLine &over, Astraddle at) {
  Placed on;
  if (!over.At(at.AlongM, on)) { return {}; }
  return Surface(on, at);
}

} // namespace outshine
