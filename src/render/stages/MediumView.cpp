#include "MediumView.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <expected>

namespace outshine::Render {
namespace {
template <typename Values> bool Finite(const Values &values) noexcept {
  return std::ranges::all_of(values, [](auto value) { return std::isfinite(value); });
}

bool ValidBasis(const CameraBasis &basis) noexcept {
  if (!Finite(basis.Right) || !Finite(basis.Up) || !Finite(basis.Forward)) { return false; }
  constexpr double kBasisTolerance = 1e-4;
  return std::abs(Dot(basis.Right, basis.Right) - 1) < kBasisTolerance &&
         std::abs(Dot(basis.Up, basis.Up) - 1) < kBasisTolerance &&
         std::abs(Dot(basis.Forward, basis.Forward) - 1) < kBasisTolerance &&
         std::abs(Dot(basis.Right, basis.Up)) < kBasisTolerance &&
         std::abs(Dot(basis.Right, basis.Forward)) < kBasisTolerance &&
         std::abs(Dot(basis.Up, basis.Forward)) < kBasisTolerance;
}

bool ValidViewport(Native::Viewport area) noexcept {
  return Finite(
             std::array{area.X, area.Y, area.Width, area.Height, area.MinDepth, area.MaxDepth}) &&
         area.X >= 0 && area.Y >= 0 && area.Width > 0 && area.Height > 0 && area.MinDepth >= 0 &&
         area.MaxDepth <= 1 && area.MaxDepth > area.MinDepth;
}
}

std::expected<MediumViewParameters, MediumViewError>
MakeMediumViewParameters(const Lens &lens,
                         Native::Viewport viewport,
                         const CameraBasis &basis,
                         SkyStanding sky,
                         const Medium &air) noexcept {
  if (!ValidViewport(viewport)) { return std::unexpected(MediumViewError::InvalidViewport); }
  const bool projected = lens.OrthoM > 0 || lens.FovDeg > 0;
  if (!std::isfinite(lens.WidePx) || !std::isfinite(lens.HighPx) || lens.WidePx <= 0 ||
      lens.HighPx <= 0 || !projected) {
    return std::unexpected(MediumViewError::InvalidProjection);
  }
  const Mat4f projection = lens.Projection();
  if (!Finite(projection) || projection[0] <= 0 || projection[5] <= 0 || projection[14] <= 0) {
    return std::unexpected(MediumViewError::InvalidProjection);
  }
  if (!ValidBasis(basis) || !Finite(sky.WorldUp) || !Finite(sky.SunDir) ||
      !Normalise(sky.WorldUp) || !Normalise(sky.SunDir)) {
    return std::unexpected(MediumViewError::InvalidBasis);
  }
  if (!Finite(std::bit_cast<std::array<float, kMediumBytes / sizeof(float)>>(air)) ||
      air.BottomRadiusKm <= 0 || air.TopRadiusKm <= air.BottomRadiusKm ||
      air.RayleighScaleHeightKm <= 0 || air.MieScaleHeightKm <= 0 || air.OzoneHalfWidthKm <= 0 ||
      !std::isfinite(sky.EyeHeightM) || !std::isfinite(sky.IlluminanceLux) ||
      sky.IlluminanceLux < 0) {
    return std::unexpected(MediumViewError::InvalidMedium);
  }
  MediumViewParameters result{};
  for (size_t axis = 0; axis < 3; ++axis) {
    result.Right[axis] = static_cast<float>(basis.Right[axis]);
    result.Up[axis] = static_cast<float>(basis.Up[axis]);
    result.Fwd[axis] = static_cast<float>(basis.Forward[axis]);
    result.WorldUp[axis] = sky.WorldUp[axis];
    result.SunDir[axis] = sky.SunDir[axis];
  }
  const bool ortho = lens.OrthoM > 0;
  const float halfWidth = 1 / projection[0];
  const float halfHeight = 1 / projection[5];
  result.TanHalf = ortho ? Vec2f{} : Vec2f{{halfWidth, halfHeight}};
  result.OrthoProjection = ortho ? Vec4f{{halfWidth, halfHeight, 1, 0}} : Vec4f{};
  const float scaleX = 2 / viewport.Width;
  const float scaleY = -2 / viewport.Height;
  const float shiftX = ortho ? projection[12] : -projection[8];
  const float shiftY = ortho ? projection[13] : -projection[9];
  result.PixelToNdc = {
      {scaleX, scaleY, -1 - viewport.X * scaleX - shiftX, 1 - viewport.Y * scaleY - shiftY}};
  const float range = viewport.MaxDepth - viewport.MinDepth;
  result.DepthReconstruction = {{projection[14] * range + viewport.MinDepth * projection[15],
                                 projection[15],
                                 projection[10] * range + viewport.MinDepth * projection[11],
                                 -projection[11]}};
  result.Illuminance = sky.IlluminanceLux;
  constexpr float kKmPerM = 0.001f;
  result.EyeRadiusKm = air.BottomRadiusKm + sky.EyeHeightM * kKmPerM;
  result.Air = air;
  if (!Finite(result.PixelToNdc) || !Finite(result.DepthReconstruction) ||
      !Finite(result.TanHalf) || !Finite(result.OrthoProjection) ||
      !std::isfinite(result.EyeRadiusKm) || result.EyeRadiusKm <= 0) {
    return std::unexpected(MediumViewError::InvalidProjection);
  }
  return result;
}
}
