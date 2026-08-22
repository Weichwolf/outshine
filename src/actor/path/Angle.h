#ifndef ANGLE_H
#define ANGLE_H

#include <numbers>

namespace outshine {

inline constexpr double kTurn = 2.0 * std::numbers::pi;

[[nodiscard]] inline double Wrapped(double angleRad) {
  while (angleRad > 0.5 * kTurn) { angleRad -= kTurn; }
  while (angleRad < -0.5 * kTurn) { angleRad += kTurn; }
  return angleRad;
}

}
#endif
