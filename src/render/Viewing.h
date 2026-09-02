#ifndef OUTSHINE_RENDER_VIEWING_H
#define OUTSHINE_RENDER_VIEWING_H

#include <span>

#include "Units.h"
#include "math/Vec3.h"
#include <array>
#include <cmath>
#include "scenario/Scenario.h"
#include <numbers>
#include <cstdint>

namespace outshine::Render {

enum class CameraKind : uint8_t { Perspective, Orthographic };

struct CameraBasis {
  Vec3 EyeM = {{0, 0, 0}};
  Vec3 Forward = {{0, 0, -1}};
  Vec3 Right = {{1, 0, 0}};
  Vec3 Up = {{0, 1, 0}};
};

struct Viewpoint : CameraBasis {
  CameraKind Kind = CameraKind::Perspective;
  double YfovRad = 0;
  double XMagM = 0;
  double YMagM = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  [[nodiscard]] static bool
  LookAt(const Vec3 &eyeM, const Vec3 &aimM, double rollRad, Viewpoint &out);
  [[nodiscard]] static bool
  LookAt(const Vec3 &eyeM, const Vec3 &aimM, const Vec3 &upM, Viewpoint &out);
};

inline bool Viewpoint::LookAt(const Vec3 &eyeM, const Vec3 &aimM, double rollRad, Viewpoint &out) {
  Vec3 forward = aimM - eyeM;
  if (!Normalise(forward)) { return false; }
  const Vec3 worldUp = {{0, 1, 0}};
  Vec3 right = Cross(forward, worldUp);
  if (!Normalise(right)) { return false; }
  const Vec3 up = Cross(right, forward);

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

inline bool Viewpoint::LookAt(const Vec3 &eyeM, const Vec3 &aimM, const Vec3 &upM, Viewpoint &out) {
  Vec3 forward = aimM - eyeM;
  if (!Normalise(forward)) { return false; }
  Vec3 right = Cross(forward, upM);
  if (!Normalise(right)) { return false; }
  const Vec3 up = Cross(right, forward);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis];
    out.Up[axis] = up[axis];
  }
  return true;
}

inline void CameraOf(const Viewpoint &from, outshine::Scenario::Camera &out) {
  out.Placed = true;
  out.LooksAt = true;
  for (int axis = 0; axis < 3; ++axis) {
    out.Stands.AtM[axis] = from.EyeM[axis];
    out.LookAtM[axis] = from.EyeM[axis] + from.Forward[axis];
    out.UpM[axis] = from.Up[axis];
  }
  if (from.Kind == CameraKind::Orthographic) {
    out.setProjection(outshine::Scenario::Camera::Ortho{
        .XMagM = from.XMagM, .YMagM = from.YMagM, .NearM = from.ZNearM, .FarM = from.ZFarM});
  } else {
    out.setProjection(outshine::Scenario::Camera::Perspective{
        .FovDeg = from.YfovRad * kRad2Deg, .NearM = from.ZNearM, .FarM = from.ZFarM});
  }
}

} // namespace outshine::Render
#endif
