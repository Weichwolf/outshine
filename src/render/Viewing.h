#ifndef OUTSHINE_RENDER_VIEWING_H
#define OUTSHINE_RENDER_VIEWING_H

#include <cmath>
#include "Scenario.h"
#include <numbers>
#include <cstdint>

namespace outshine::Render {

enum class CameraKind : uint8_t { Perspective, Orthographic };

struct Viewpoint {
  double EyeM[3] = {0, 0, 0};
  double Forward[3] = {0, 0, -1};
  double Right[3] = {1, 0, 0};
  double Up[3] = {0, 1, 0};

  CameraKind Kind = CameraKind::Perspective;
  double YfovRad = 0;
  double XMagM = 0;
  double YMagM = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  [[nodiscard]] static bool
  LookAt(const double eyeM[3], const double aimM[3], double rollRad, Viewpoint &out);
  [[nodiscard]] static bool
  LookAt(const double eyeM[3], const double aimM[3], const double upM[3], Viewpoint &out);
};

namespace Aiming {

inline bool Normalise(double v[3]) {
  const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (!(len > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= len; }
  return true;
}

inline void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

}

inline bool
Viewpoint::LookAt(const double eyeM[3], const double aimM[3], double rollRad, Viewpoint &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Aiming::Normalise(forward)) { return false; }
  const double worldUp[3] = {0, 1, 0};
  double right[3];
  Aiming::Cross(forward, worldUp, right);
  if (!Aiming::Normalise(right)) { return false; }
  double up[3];
  Aiming::Cross(right, forward, up);

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

inline bool
Viewpoint::LookAt(const double eyeM[3], const double aimM[3], const double upM[3], Viewpoint &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Aiming::Normalise(forward)) { return false; }
  double right[3];
  Aiming::Cross(forward, upM, right);
  if (!Aiming::Normalise(right)) { return false; }
  double up[3];
  Aiming::Cross(right, forward, up);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis];
    out.Up[axis] = up[axis];
  }
  return true;
}

inline void CameraOf(const Viewpoint &from, outshine::Camera &out) {
  out.Placed = true;
  out.LooksAt = true;
  for (int axis = 0; axis < 3; ++axis) {
    out.Stands.AtM[axis] = from.EyeM[axis];
    out.LookAtM[axis] = from.EyeM[axis] + from.Forward[axis];
    out.UpM[axis] = from.Up[axis];
  }
  if (from.Kind == CameraKind::Orthographic) {
    out.setProjection(-from.XMagM, from.XMagM, -from.YMagM, from.YMagM, from.ZNearM, from.ZFarM);
  } else {
    out.setProjection(from.YfovRad * 180.0 / std::numbers::pi, from.ZNearM, from.ZFarM);
  }
}

}
#endif
