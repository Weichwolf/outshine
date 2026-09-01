#ifndef OUTSHINE_BASE_MATH_VEC3_H
#define OUTSHINE_BASE_MATH_VEC3_H

#include <cmath>
#include <span>

namespace outshine {

[[nodiscard]] inline double Dot(std::span<const double, 3> a, std::span<const double, 3> b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void
Cross(std::span<const double, 3> a, std::span<const double, 3> b, std::span<double, 3> out) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

[[nodiscard]] inline double Length(std::span<const double, 3> v) {
  return std::sqrt(Dot(v, v));
}

[[nodiscard]] inline bool Normalise(std::span<double, 3> v) {
  const double length = Length(v);
  if (!(length > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

} // namespace outshine

#endif
