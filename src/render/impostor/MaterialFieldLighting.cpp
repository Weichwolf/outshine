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
