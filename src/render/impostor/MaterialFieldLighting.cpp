#include "MaterialFieldPassState.h"
#include "LightingUniform.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace outshine::Render {
namespace {
bool Nonnegative(const auto &values) {
  return std::ranges::all_of(values,
                             [](double value) { return std::isfinite(value) && value >= 0; });
}

bool Finite(const auto &values) {
  return std::ranges::all_of(values, [](double value) { return std::isfinite(value); });
}
}

bool ValidMaterialFieldCameraBasis(const Viewpoint &camera) noexcept {
  return (camera.Kind == CameraKind::Perspective || camera.Kind == CameraKind::Orthographic) &&
         Finite(camera.EyeM) && Finite(camera.Right) && Finite(camera.Up) &&
         Finite(camera.Forward) &&
         std::abs(Dot(camera.Right, camera.Right) - 1) <= kMaterialFieldUnitTolerance &&
         std::abs(Dot(camera.Up, camera.Up) - 1) <= kMaterialFieldUnitTolerance &&
         std::abs(Dot(camera.Forward, camera.Forward) - 1) <= kMaterialFieldUnitTolerance &&
         Dot(Cross(camera.Right, camera.Up), camera.Forward) <= -1 + kMaterialFieldUnitTolerance;
}

bool SameMaterialFieldCapture(const Viewpoint &capture, const Viewpoint &view) noexcept {
  if (capture.Kind != CameraKind::Orthographic || view.Kind != capture.Kind ||
      capture.XMagM != view.XMagM || capture.YMagM != view.YMagM || capture.Right != view.Right ||
      capture.Up != view.Up || capture.Forward != view.Forward) {
    return false;
  }
  const Vec3 offset = view.EyeM - capture.EyeM;
  const double forward = Dot(offset, capture.Forward);
  return Dot(offset, capture.Right) == 0 && Dot(offset, capture.Up) == 0 &&
         view.ZNearM + forward == capture.ZNearM && view.ZFarM + forward == capture.ZFarM;
}

bool ValidMaterialFieldLighting(const MaterialFieldView &view) noexcept {
  const auto &environment = view.Environment;
  if (view.Lights.size() > kMaxUniformLights || !Nonnegative(environment.RadianceLinear) ||
      !Nonnegative(environment.GroundLinear) || !Nonnegative(environment.GroundAlbedo) ||
      !Finite(environment.UpUnit) ||
      std::abs(Dot(environment.UpUnit, environment.UpUnit) - 1) > kMaterialFieldUnitTolerance ||
      !std::isfinite(environment.SkyLux) || environment.SkyLux < 0 ||
      !std::isfinite(environment.CosSunZenith) || std::abs(environment.CosSunZenith) > 1) {
    return false;
  }
  return std::ranges::all_of(view.Lights, [](const SceneLight &placed) {
    const auto &light = placed.Light;
    if (light.Kind < LightKind::Directional || light.Kind > LightKind::Spot ||
        !Nonnegative(light.Colour) || !std::isfinite(light.Intensity) || light.Intensity < 0 ||
        !Finite(placed.PositionM) || !Finite(light.Direction) || !std::isfinite(light.RangeM) ||
        light.RangeM < 0 || !std::isfinite(light.InnerConeRad) ||
        !std::isfinite(light.OuterConeRad)) {
      return false;
    }
    if (light.Kind != LightKind::Point &&
        std::abs(Dot(light.Direction, light.Direction) - 1) > kMaterialFieldUnitTolerance) {
      return false;
    }
    if (light.Kind == LightKind::Spot &&
        (light.InnerConeRad < 0 || light.OuterConeRad < light.InnerConeRad ||
         light.OuterConeRad > std::numbers::pi_v<float> / 2)) {
      return false;
    }
    return true;
  });
}
}
