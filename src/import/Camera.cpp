#include "math/Units.h"
#include <cmath>
#include "math/Mat4.h"
#include "scenario/Scenario.h"
#include "Subject.h"
#include "Viewing.h"

namespace outshine {

bool Scenario::Camera::modelMatrix(Mat4 &out) const {
  if (Stands.GlobeAnchor) { return false; }
  for (const double axis : Stands.AtM) {
    if (!std::isfinite(axis)) { return false; }
  }
  if (!LooksAt) {
    const Quat &q = Stands.Facing;
    const double norm2 = q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W;
    if (!(norm2 > 0.0) || !std::isfinite(norm2)) { return false; }
    out = Gltf::Transform::FromTrs(Stands.AtM, Stands.Facing, {{1, 1, 1}}).M;
    return true;
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(LookAtM[axis]) || !std::isfinite(UpM[axis])) { return false; }
  }
  const auto basis = Render::Viewpoint::LookAt({.EyeM = Stands.AtM, .AimM = LookAtM}, UpM);
  if (!basis) { return false; }
  out = Mat4{};
  for (int axis = 0; axis < 3; ++axis) {
    out[axis] = basis->Right[axis];
    out[4 + axis] = basis->Up[axis];
    out[8 + axis] = -basis->Forward[axis];
    out[12 + axis] = basis->EyeM[axis];
  }
  return true;
}

bool Scenario::Camera::viewMatrix(Mat4 &out) const {
  Mat4 model;
  if (!modelMatrix(model)) { return false; }
  Gltf::Transform inverse;
  if (!Gltf::Transform::FromColumnMajor(model).Inverse(inverse)) { return false; }
  out = inverse.M;
  return true;
}

bool Scenario::Camera::projectionMatrix(double aspect, Mat4 &out) const {
  Gltf::Camera lens;
  lens.Kind = Orthographic ? Gltf::CameraKind::Orthographic : Gltf::CameraKind::Perspective;
  lens.YfovRad = FovDeg * kDeg2Rad;
  lens.XMagM = XMagM;
  lens.YMagM = YMagM;
  lens.ZNearM = NearM;
  lens.ZFarM = FarM;
  Gltf::Transform made;
  if (!lens.Projection(aspect, made)) { return false; }
  out = made.M;
  return true;
}

bool Scenario::Camera::clipMatrix(double aspect, Mat4 &out) const {
  Mat4 view;
  Mat4 projection;
  if (!viewMatrix(view) || !projectionMatrix(aspect, projection)) { return false; }
  out = projection * view;
  return true;
}

} // namespace outshine
