#include "CameraPreparation.h"
#include "GeodeticCamera.h"
#include "Mat4.h"
#include "Scenario.h"
#include "Units.h"
#include <expected>
#include <optional>
#include <string_view>

namespace outshine {
std::expected<Render::Viewpoint, Render::LensError>
ProjectCamera(const Camera &camera, Render::Viewpoint view, double widthPx, double heightPx) {
  view.Kind =
      camera.Orthographic ? Render::CameraKind::Orthographic : Render::CameraKind::Perspective;
  view.YfovRad = (camera.FovDeg == 0 ? Scenario::kFovUnsaidDeg : camera.FovDeg) * kDeg2Rad;
  view.ZNearM = !camera.Orthographic && camera.NearM == 0 ? Camera::kNearestM : camera.NearM;
  view.ZFarM = camera.FarM;
  view.XMagM = camera.XMagM;
  view.YMagM = camera.YMagM;
  const auto lens = Render::Lens::From(view, widthPx, heightPx);
  if (!lens) { return std::unexpected(lens.error()); }
  return view;
}

std::expected<std::optional<Render::Viewpoint>, std::string_view>
PrepareCamera(const Scenario::View &seen, const CameraPreparationContext &context) {
  if (seen.Placement != Scenario::CameraPlacement::Local &&
      seen.Placement != Scenario::CameraPlacement::Geodetic) {
    return std::unexpected("static camera preparation requires a local or geodetic view");
  }
  Vec3 station = seen.Sees.PositionM + seen.OffsetM;
  if (seen.Placement == Scenario::CameraPlacement::Geodetic) {
    const auto position =
        ResolveGeodeticCamera(seen, context.Origin, context.Ground, context.HeightSample);
    if (!position) { return std::unexpected(position.error()); }
    if (!*position) { return std::optional<Render::Viewpoint>{}; }
    station = **position;
  }
  Camera resolved = seen.Sees;
  resolved.PositionM = station;
  if (seen.Placement == Scenario::CameraPlacement::Geodetic && !seen.Sees.LooksAt) {
    const auto axes = ResolveGeodeticCameraAxes(seen.Geographic, context.Origin);
    if (!axes) { return std::unexpected(axes.error()); }
    resolved.LooksAt = true;
    resolved.LookAtM = station + axes->Forward;
    resolved.UpM = axes->Up;
  }
  Mat4 model;
  if (!resolved.modelMatrix(model)) { return std::unexpected(kInvalidCameraTransform); }
  Render::Viewpoint standing;
  standing.EyeM = station;
  for (int axis = 0; axis < 3; ++axis) {
    standing.Right[axis] = model[axis];
    standing.Up[axis] = model[4 + axis];
    standing.Forward[axis] = -model[8 + axis];
  }
  auto projected = ProjectCamera(seen.Sees, standing, context.WidthPx, context.HeightPx);
  if (!projected) { return std::unexpected(kInvalidCameraProjection); }
  return *projected;
}
}
