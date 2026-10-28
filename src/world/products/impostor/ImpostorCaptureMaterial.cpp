#include "ImpostorCaptureMaterial.h"
#include <algorithm>
#include <array>
#include <optional>

namespace outshine {
std::optional<Material> CapturedImpostorMaterial(const Material &source) {
  Material supported;
  supported.Scattering = source.Scattering;
  supported.DiffuseTransmission = source.DiffuseTransmission;
  supported.Pattern = source.Pattern;
  supported.BaseColour = source.BaseColour;
  supported.Metalness = source.Metalness;
  supported.Roughness = source.Roughness;
  supported.Alpha = source.Alpha;
  supported.CoverageCut = source.CoverageCut;
  supported.DoubleSided = source.DoubleSided;
  supported.NormalScale = source.NormalScale;
  supported.NeedsTangents = source.NeedsTangents;
  supported.BaseColourMap = source.BaseColourMap;
  supported.NormalMap = source.NormalMap;
  supported.MetalRoughMap = source.MetalRoughMap;
  const std::array maps{source.BaseColourMap, source.NormalMap, source.MetalRoughMap};
  if (!(source == supported) || source.Alpha == AlphaMode::Blended ||
      !std::ranges::all_of(
          maps, [](const SurfaceMap &map) { return !map.bound() || map.Set == UvSet::Uv0; })) {
    return std::nullopt;
  }
  supported.Pattern = SurfacePattern::None;
  supported.NormalScale = 1;
  supported.NeedsTangents = false;
  supported.BaseColourMap = supported.NormalMap = supported.MetalRoughMap = {};
  return supported;
}

}
