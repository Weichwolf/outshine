#include <numbers>

#include "Units.h"
#include "math/Mat4.h"
#include "scenario/Scenario.h"
#include "Subject.h"
#include "Viewing.h"

namespace outshine {

namespace {

[[nodiscard]] bool StandingOf(const Scenario::Camera &from, Render::Viewpoint &out) {
  if (!Render::Viewpoint::LookAt(from.Stands.AtM, from.LookAtM, from.UpM, out)) { return false; }
  out.ZNearM = from.NearM;
  out.ZFarM = from.FarM;
  if (from.Orthographic) {
    out.Kind = Render::CameraKind::Orthographic;
    out.XMagM = from.XMagM;
    out.YMagM = from.YMagM;
  } else {
    out.Kind = Render::CameraKind::Perspective;
    out.YfovRad = from.FovDeg * kDeg2Rad;
  }
  return true;
}

} // namespace

bool Scenario::Camera::viewMatrix(Mat4 &out) const {
  Render::Viewpoint standing;
  Gltf::Transform made;
  if (!StandingOf(*this, standing) || !Gltf::ViewOf(standing, made)) { return false; }
  for (int at = 0; at < 16; ++at) { out[at] = made.M[at]; }
  return true;
}

bool Scenario::Camera::projectionMatrix(double aspect, Mat4 &out) const {
  Render::Viewpoint standing;
  if (!StandingOf(*this, standing)) { return false; }
  Gltf::Camera lens;
  lens.Kind = standing.Kind == Render::CameraKind::Orthographic ? Gltf::CameraKind::Orthographic
                                                                : Gltf::CameraKind::Perspective;
  lens.YfovRad = standing.YfovRad;
  lens.XMagM = standing.XMagM;
  lens.YMagM = standing.YMagM;
  lens.ZNearM = standing.ZNearM;
  lens.ZFarM = standing.ZFarM;
  Gltf::Transform made;
  if (!lens.Projection(aspect, made)) { return false; }
  for (int at = 0; at < 16; ++at) { out[at] = made.M[at]; }
  return true;
}

bool Scenario::Camera::clipMatrix(double aspect, Mat4 &out) const {
  Render::Viewpoint standing;
  Gltf::Transform made;
  if (!StandingOf(*this, standing) || !Gltf::ClipOf(standing, aspect, made)) { return false; }
  for (int at = 0; at < 16; ++at) { out[at] = made.M[at]; }
  return true;
}

} // namespace outshine
