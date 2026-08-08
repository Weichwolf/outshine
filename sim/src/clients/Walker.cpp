#include "Walker.h"

#include <cmath>

#include "Camera.h"
#include "Units.h"

namespace outshine::Clients {

/* One integration step: look first, then walk in the plane the new heading defines. */
const Outshine::Stance &Walker::Step(double dtS) {
  if (dtS > kMaxStepS) dtS = kMaxStepS;
  if (LookX_ != 0.0 || LookY_ != 0.0) {
    At_.YawDeg = Wrap180(At_.YawDeg + LookX_ * kMouseDegPerPx);
    At_.PitchDeg -= LookY_ * kMouseDegPerPx;   /* screen-down is +Y, and pushing the mouse down looks down */
    if (At_.PitchDeg > kMaxPitchDeg) At_.PitchDeg = kMaxPitchDeg;
    if (At_.PitchDeg < -kMaxPitchDeg) At_.PitchDeg = -kMaxPitchDeg;
    LookX_ = LookY_ = 0.0;
  }

  const double f = (Held_[(int)Move::Fwd] ? 1.0 : 0.0) - (Held_[(int)Move::Back] ? 1.0 : 0.0);
  const double r = (Held_[(int)Move::Right] ? 1.0 : 0.0) - (Held_[(int)Move::Left] ? 1.0 : 0.0);
  if (f == 0.0 && r == 0.0) return At_;
  const double len = std::sqrt(f * f + r * r);   /* diagonals are not faster */
  const double step = kWalkSpeedMs * (Fast_ ? kRunFactor : 1.0) * dtS / len;
  /* THE VIEW PLANE IS HORIZONTAL: walking is yaw-only, so looking up does not lift the walker off
   * the ground. Yaw 0 = north, 90 = east (core/Camera.h's convention). */
  const double yaw = At_.YawDeg * kDeg2Rad;
  const double north = (f * std::cos(yaw) - r * std::sin(yaw)) * step;
  const double east = (f * std::sin(yaw) + r * std::cos(yaw)) * step;
  At_.Lat += north / kMPerDeg;
  const double coslat = std::cos(At_.Lat * kDeg2Rad);
  At_.Lon += east / (kMPerDeg * (coslat > 1e-6 ? coslat : 1e-6));
  return At_;
}

} // namespace outshine::Clients
