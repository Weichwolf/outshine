#ifndef OUTSHINE_BASE_MATH_VEC3_H
#define OUTSHINE_BASE_MATH_VEC3_H

#include <cmath>

namespace outshine {

[[nodiscard]] inline double Dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

[[nodiscard]] inline double Length(const double v[3]) {
  return std::sqrt(Dot(v, v));
}

[[nodiscard]] inline bool Normalise(double v[3]) {
  const double length = Length(v);
  if (!(length > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

} // namespace outshine

#endif
