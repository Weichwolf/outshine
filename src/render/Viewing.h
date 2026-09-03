#ifndef OUTSHINE_RENDER_VIEWING_H
#define OUTSHINE_RENDER_VIEWING_H

#include <span>
#include <optional>

#include "math/Units.h"
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

  struct Looking {
    Vec3 EyeM;
    Vec3 AimM;
  };

  [[nodiscard]] static std::optional<Viewpoint> LookAt(Looking from, double rollRad);

  [[nodiscard]] static std::optional<Viewpoint> LookAt(Looking from, const Vec3 &upM);
};

inline std::optional<Viewpoint> Viewpoint::LookAt(Looking from, double rollRad) {
  Vec3 forward = from.AimM - from.EyeM;
  if (!Normalise(forward)) { return std::nullopt; }
  const Vec3 worldUp = {{0, 1, 0}};
  Vec3 right = Cross(forward, worldUp);
  if (!Normalise(right)) { return std::nullopt; }
  const Vec3 up = Cross(right, forward);

  const double turn = std::cos(rollRad);
  const double lean = std::sin(rollRad);
  Viewpoint out;
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = from.EyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis] * turn + up[axis] * lean;
    out.Up[axis] = up[axis] * turn - right[axis] * lean;
  }
  return out;
}

inline std::optional<Viewpoint> Viewpoint::LookAt(Looking from, const Vec3 &upM) {
  Vec3 forward = from.AimM - from.EyeM;
  if (!Normalise(forward)) { return std::nullopt; }
  Vec3 right = Cross(forward, upM);
  if (!Normalise(right)) { return std::nullopt; }
  const Vec3 up = Cross(right, forward);
  Viewpoint out;
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = from.EyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis];
    out.Up[axis] = up[axis];
  }
  return out;
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
