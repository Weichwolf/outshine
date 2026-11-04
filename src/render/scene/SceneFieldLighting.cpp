#include "SceneRenderer.h"
#include "MaterialFieldPass.h"
#include "SceneMaterialFieldView.h"
#include "TransformMatrix.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <expected>
#include <memory>
#include <vector>

namespace outshine::Render {
namespace {
constexpr double kRigidFrameTolerance = 1e-9;

Native::Result<Mat4> FieldFromScene(const Mat4 &matrix) {
  if (!std::ranges::all_of(matrix, [](double value) { return std::isfinite(value); }) ||
      matrix[3] != 0 || matrix[7] != 0 || matrix[11] != 0 || matrix[15] != 1) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "material field needs a finite rigid scene frame"});
  }
  const std::array basis{Vec3{{matrix[0], matrix[1], matrix[2]}},
                         Vec3{{matrix[4], matrix[5], matrix[6]}},
                         Vec3{{matrix[8], matrix[9], matrix[10]}}};
  for (size_t a = 0; a < basis.size(); ++a) {
    for (size_t b = 0; b < basis.size(); ++b) {
      if (std::abs(Dot(basis[a], basis[b]) - (a == b ? 1.0 : 0.0)) > kRigidFrameTolerance) {
        return std::unexpected(
            Native::Error{.Kind = Native::ErrorKind::Invalid,
                          .Message = "material field scene frame has scale or shear"});
      }
    }
  }
  Mat4 inverse;
  if (Dot(Cross(basis[0], basis[1]), basis[2]) < 0 || !InverseMatrix(matrix, inverse)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "material field scene frame changes handedness"});
  }
  return inverse;
}

Vec3f Direction(const Mat4 &frame, Vec3f direction) {
  const auto mapped = frame.TransformDirection({{direction[0], direction[1], direction[2]}});
  return {{static_cast<float>(mapped[0]),
           static_cast<float>(mapped[1]),
           static_cast<float>(mapped[2])}};
}
}

Native::Result<MaterialFieldImages>
SceneRenderer::RelightSceneMaterialField(const ResidentMaterialField &source,
                                         const SceneMaterialFieldView &view,
                                         const std::atomic_bool *stopping) {
  if (!Device_ || !ActiveState().Ready || !MaterialFields_) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "scene material field requires a prepared world"});
  }
  const auto frame = FieldFromScene(view.SceneFromField);
  if (!frame) { return std::unexpected(frame.error()); }
  const auto &geometry = ActiveContent().Geometry();
  std::vector<SceneLight> lights(geometry.Lights().begin(), geometry.Lights().end());
  for (auto &light : lights) {
    light.PositionM = frame->TransformPoint(light.PositionM);
    light.Light.Direction = Direction(*frame, light.Light.Direction);
  }
  MaterialFieldView placed;
  placed.Camera = view.Capture;
  placed.Target = view.Target;
  placed.Encoding = view.Encoding;
  placed.Lights = lights;
  placed.Environment = geometry.Environment();
  placed.Environment.UpUnit = frame->TransformDirection(placed.Environment.UpUnit);
  Viewpoint observer;
  observer.Kind = ActiveState().OrthoM > 0 || ActiveState().OrthoWidthM > 0
                      ? CameraKind::Orthographic
                      : CameraKind::Perspective;
  observer.EyeM = frame->TransformPoint(ActiveState().Camera.EyeM);
  observer.Right = frame->TransformDirection(ActiveState().Camera.Right);
  observer.Up = frame->TransformDirection(ActiveState().Camera.Up);
  observer.Forward = frame->TransformDirection(ActiveState().Camera.Forward);
  placed.LightingCamera = observer;
  if (ActiveState().SkyDeclared && ActiveState().MediumDeclared &&
      ActiveFrame().Atmosphere.Has(Resource::EnvironmentSpecular)) {
    ApplyWorldDeclarations();
    const auto &atmosphere = ActiveFrame().Atmosphere;
    const auto produced = Uploads_->LastSubmitted();
    if (!atmosphere.Settled(Stage::EnvironmentSpecular) || !produced) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                           .Message = "scene reflection field is not submitted"});
    }
    placed.Reflections = MaterialFieldView::ReflectedEnvironment{
        .Atlas = std::make_shared<RadianceImage>(
            RadianceImage{.Device = Device_.get(),
                          .Texture = atmosphere.TextureLease(Resource::EnvironmentSpecular),
                          .Sampling = atmosphere.SamplerLease(),
                          .Produced = *produced}),
        .Up = Direction(*frame, ActiveState().SkyUp),
        .ToSun = Direction(*frame, ActiveState().SkyToSun)};
  }
  return RelightMaterialField(source, placed, stopping);
}
}
