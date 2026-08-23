#ifndef OUTSHINE_ACTOR_PATH_ANGLE_H
#define OUTSHINE_ACTOR_PATH_ANGLE_H

#include <numbers>

namespace outshine {

inline constexpr double kTurn = 2.0 * std::numbers::pi;

[[nodiscard]] constexpr double Wrapped(double angleRad) noexcept {
  while (angleRad > 0.5 * kTurn) { angleRad -= kTurn; }
  while (angleRad < -0.5 * kTurn) { angleRad += kTurn; }
  return angleRad;
}

}
#endif
