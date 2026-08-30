#include <numbers>

#include "Scenario.h"
#include "Subject.h"
#include "Viewing.h"

namespace outshine {

namespace {

[[nodiscard]] bool StandingOf(const Camera &from, Render::Viewpoint &out) {
  const double aim[3] = {from.LookAtM[0], from.LookAtM[1], from.LookAtM[2]};
  if (!Render::Viewpoint::LookAt(from.Stands.AtM, aim, from.UpM, out)) { return false; }
  out.ZNearM = from.NearM;
  out.ZFarM = from.FarM;
  if (from.Orthographic) {
    out.Kind = Render::CameraKind::Orthographic;
    out.XMagM = from.XMagM;
    out.YMagM = from.YMagM;
  } else {
    out.Kind = Render::CameraKind::Perspective;
    out.YfovRad = from.FovDeg * std::numbers::pi / 180.0;
  }
  return true;
}

} // namespace

bool Camera::viewMatrix(double outM16[16]) const {
  Render::Viewpoint standing;
  Gltf::Transform made;
  if (!StandingOf(*this, standing) || !Gltf::ViewOf(standing, made)) { return false; }
  for (int at = 0; at < 16; ++at) { outM16[at] = made.M[at]; }
  return true;
}

bool Camera::projectionMatrix(double aspect, double outM16[16]) const {
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
  for (int at = 0; at < 16; ++at) { outM16[at] = made.M[at]; }
  return true;
}

bool Camera::clipMatrix(double aspect, double outM16[16]) const {
  Render::Viewpoint standing;
  Gltf::Transform made;
  if (!StandingOf(*this, standing) || !Gltf::ClipOf(standing, aspect, made)) { return false; }
  for (int at = 0; at < 16; ++at) { outM16[at] = made.M[at]; }
  return true;
}

} // namespace outshine
