#include "math/Units.h"
#include "math/Projection.h"
#include <cmath>
#include <algorithm>
#include <expected>
#include "math/Mat4.h"
#include "render/Camera.h"
#include "math/TransformMatrix.h"
#include "Viewing.h"

namespace outshine {

namespace {
constexpr double kExposureCalibration = 120.0;

[[nodiscard]] bool PublishMatrix(const Mat4 &candidate, Mat4 &out) noexcept {
  if (!std::ranges::all_of(candidate, [](double value) { return std::isfinite(value); })) {
    return false;
  }
  out = candidate;
  return true;
}
}

double Camera::exposureScale() const noexcept {
  if (!exposed()) { return 0.0; }
  const double logScale = std::log2(ShutterS) + std::log2(SensitivityIso) -
                          2 * std::log2(ApertureFStops) - std::log2(kExposureCalibration);
  const double scale = std::exp2(logScale);
  return std::isfinite(scale) && scale > 0.0 ? scale : 0.0;
}

std::expected<void, CameraMatrixError> Camera::modelMatrix(Mat4 &out) const noexcept {
  for (const double axis : PositionM) {
    if (!std::isfinite(axis)) { return std::unexpected(CameraMatrixError::InvalidPose); }
  }
  if (!LooksAt) {
    const Quat &q = Orientation;
    const double norm2 = q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W;
    if (!(norm2 > 0.0) || !std::isfinite(norm2)) {
      return std::unexpected(CameraMatrixError::InvalidPose);
    }
    out = TransformMatrix(PositionM, Orientation, {{1, 1, 1}});
    return {};
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(LookAtM[axis]) || !std::isfinite(UpM[axis])) {
      return std::unexpected(CameraMatrixError::InvalidPose);
    }
  }
  const auto basis = Render::Viewpoint::LookAt({.EyeM = PositionM, .AimM = LookAtM}, UpM);
  if (!basis) { return std::unexpected(CameraMatrixError::InvalidPose); }
  out = Mat4{};
  for (int axis = 0; axis < 3; ++axis) {
    out[axis] = basis->Right[axis];
    out[4 + axis] = basis->Up[axis];
    out[8 + axis] = -basis->Forward[axis];
    out[12 + axis] = basis->EyeM[axis];
  }
  return {};
}

std::expected<void, CameraMatrixError> Camera::viewMatrix(Mat4 &out) const noexcept {
  Mat4 model;
  if (const auto modelled = modelMatrix(model); !modelled) {
    return std::unexpected(modelled.error());
  }
  Mat4 candidate;
  if (!InverseMatrix(model, candidate)) {
    return std::unexpected(CameraMatrixError::Unrepresentable);
  }
  return PublishMatrix(candidate, out) ? std::expected<void, CameraMatrixError>{}
                                       : std::unexpected(CameraMatrixError::Unrepresentable);
}

std::expected<void, CameraMatrixError> Camera::projectionMatrix(double aspect,
                                                                Mat4 &out) const noexcept {
  if (Orthographic && NearM < 0.0) { return std::unexpected(CameraMatrixError::InvalidLens); }
  Mat4 candidate;
  const bool valid =
      Orthographic
          ? ProjectionMatrix(
                OrthographicProjection{
                    .HalfWidthM = XMagM, .HalfHeightM = YMagM, .NearM = NearM, .FarM = FarM},
                candidate)
          : ProjectionMatrix(PerspectiveProjection{.VerticalFovRad = FovDeg * kDeg2Rad,
                                                   .NearM = NearM,
                                                   .FarM = FarM},
                             aspect,
                             candidate);
  if (!valid) { return std::unexpected(CameraMatrixError::InvalidLens); }
  return PublishMatrix(candidate, out) ? std::expected<void, CameraMatrixError>{}
                                       : std::unexpected(CameraMatrixError::Unrepresentable);
}

std::expected<void, CameraMatrixError> Camera::clipMatrix(double aspect, Mat4 &out) const noexcept {
  Mat4 view;
  Mat4 projection;
  if (const auto viewed = viewMatrix(view); !viewed) { return std::unexpected(viewed.error()); }
  if (const auto projected = projectionMatrix(aspect, projection); !projected) {
    return std::unexpected(projected.error());
  }
  return PublishMatrix(projection * view, out)
             ? std::expected<void, CameraMatrixError>{}
             : std::unexpected(CameraMatrixError::Unrepresentable);
}

}
