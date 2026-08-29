#include "Viewing.h"

#include <cmath>

namespace outshine::Render {

namespace {

bool Normalise(double v[3]) {
  const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (!(len > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= len; }
  return true;
}

void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

}

bool Viewpoint::LookAt(const double eyeM[3], const double aimM[3], double rollRad, Viewpoint &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Normalise(forward)) { return false; }
  const double worldUp[3] = {0, 1, 0};
  double right[3];
  Cross(forward, worldUp, right);
  if (!Normalise(right)) { return false; }
  double up[3];
  Cross(right, forward, up);

  const double turn = std::cos(rollRad);
  const double lean = std::sin(rollRad);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis] * turn + up[axis] * lean;
    out.Up[axis] = up[axis] * turn - right[axis] * lean;
  }
  return true;
}

}
