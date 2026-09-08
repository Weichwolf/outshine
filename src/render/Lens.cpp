#include "Lens.h"
#include "math/Units.h"
#include <cassert>
#include <algorithm>
#include <expected>
#include <limits>
#include <numbers>
#include <cmath>

namespace outshine::Render {

namespace {
bool FitsFloat(double value) noexcept {
  return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
}

bool ValidProjection(const Viewpoint &eye) noexcept {
  const bool finiteFar = std::isfinite(eye.ZFarM) && eye.ZFarM > eye.ZNearM;
  if (!std::isfinite(eye.ZNearM)) { return false; }
  switch (eye.Kind) {
    case CameraKind::Orthographic:
      return eye.ZNearM >= 0 && finiteFar && eye.XMagM > 0 && eye.YMagM > 0 &&
             std::isfinite(eye.XMagM) && std::isfinite(eye.YMagM);
    case CameraKind::Perspective:
      return eye.ZNearM > 0 && eye.YfovRad > 0 && eye.YfovRad < std::numbers::pi &&
             (finiteFar || eye.ZFarM == 0 || eye.ZFarM == std::numeric_limits<double>::infinity());
  }
  return false;
}
} // namespace

std::expected<Lens, LensError>
Lens::From(const Viewpoint &eye, double widthPx, double heightPx) noexcept {
  if (!FitsFloat(widthPx) || !FitsFloat(heightPx) || widthPx < 1 || heightPx < 1) {
    return std::unexpected(LensError::InvalidExtent);
  }
  if (!ValidProjection(eye)) { return std::unexpected(LensError::InvalidProjection); }
  const bool ortho = eye.Kind == CameraKind::Orthographic;
  const double widthM = ortho ? 2.0 * eye.XMagM : 0.0;
  const double heightM = ortho ? 2.0 * eye.YMagM : 0.0;
  const double degrees = ortho ? 0.0 : eye.YfovRad * kRad2Deg;
  const double farM = std::isinf(eye.ZFarM) ? 0.0 : eye.ZFarM;
  if (!FitsFloat(widthM) || !FitsFloat(heightM) || !FitsFloat(degrees) || !FitsFloat(eye.ZNearM) ||
      !FitsFloat(farM)) {
    return std::unexpected(LensError::FloatRange);
  }
  const Lens lens{.WidePx = widthPx,
                  .HighPx = heightPx,
                  .FovDeg = static_cast<float>(degrees),
                  .OrthoWidthM = static_cast<float>(widthM),
                  .OrthoM = static_cast<float>(heightM),
                  .NearM = static_cast<float>(eye.ZNearM),
                  .FarM = static_cast<float>(farM)};
  const bool magnified = ortho ? lens.OrthoWidthM > 0 && lens.OrthoM > 0
                               : lens.FovDeg > 0 && lens.FovDeg < 180 && lens.NearM > 0;
  if (!magnified || (farM > 0 && lens.FarM <= lens.NearM) || (eye.ZNearM > 0 && lens.NearM == 0)) {
    return std::unexpected(LensError::FloatRange);
  }
  const Mat4f projection = lens.Projection();
  if (!std::ranges::all_of(projection, [](float value) { return std::isfinite(value); }) ||
      projection[0] <= 0 || projection[5] <= 0 || projection[14] <= 0) {
    return std::unexpected(LensError::FloatRange);
  }
  return lens;
}

Mat4f Lens::Projection() const noexcept {
  const double widePx = WidePx;
  const double highPx = HighPx;
  const float asp = static_cast<float>(widePx) / static_cast<float>(highPx);
  const float zn = NearM;
  assert(OrthoM > 0.0f || FovDeg > 0.0f);
  const float f =
      OrthoM > 0.0f ? 0.0f : 1.0f / std::tan(FovDeg * static_cast<float>(kDeg2Rad) / 2.0f);
  Mat4f p = {{f / asp, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, zn, 0}};

  if (FarM > 0.0f && std::isfinite(FarM)) {
    p[10] = zn / (FarM - zn);
    p[14] = FarM * p[10];
  }

  const float ndcX = widePx > 0 ? 2.0f * Jitter[0] / static_cast<float>(widePx) : 0.0f;
  const float ndcY = highPx > 0 ? 2.0f * Jitter[1] / static_cast<float>(highPx) : 0.0f;
  p[8] = -ndcX;
  p[9] = -ndcY;
  if (OrthoM > 0.0f) {
    const float hw = 0.5f * OrthoWidthM;
    const float hh = 0.5f * OrthoM;
    const float zf = FarM;
    const float rz = 1.0f / (zf - zn);
    Mat4f q = {{1.0f / hw, 0, 0, 0, 0, 1.0f / hh, 0, 0, 0, 0, rz, 0, 0, 0, zf * rz, 1}};
    q[12] = ndcX;
    q[13] = ndcY;
    for (int i = 0; i < 16; i++) { p[i] = q[i]; }
  }
  return p;
}

} // namespace outshine::Render
