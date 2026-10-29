#include "SceneLighting.h"
#include "GeometryRenderer.h"
#include "LightingPacking.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>

namespace outshine::Render {
bool GeometryRenderer::SetLights(std::span<const SceneLight> lights, std::string &error) {
  if (lights.size() > kMaxUniformLights) {
    error = "the geometry declares " + std::to_string(lights.size()) +
            " punctual lights over a list of " + std::to_string(kMaxUniformLights) +
            ", and a light this unit cannot bind is a refusal rather than a light left out of the "
            "picture";
    return false;
  }
  Placed.assign(lights.begin(), lights.end());
  return true;
}

static_assert(static_cast<int>(LightKind::Directional) == 0 &&
                  static_cast<int>(LightKind::Point) == 1 && static_cast<int>(LightKind::Spot) == 2,
              "the shader reads the kind as the enum's own number, so reordering LightKind moves "
              "every light to another shape");

std::array<float, GeometryRenderer::kLightFloats>
GeometryRenderer::PackedLights(const FrameContext &ctx) const {
  auto packed = PackLightingUniform(Placed, IndirectLight, ctx);
  const bool shadowSource = Shadowed_ && ShadowLightIndex_ < Placed.size() &&
                            Placed[ShadowLightIndex_].Light.Kind == LightKind::Directional;
  packed.Count[2] = shadowSource ? static_cast<float>(ShadowLightIndex_ + 1) : 0.0f;
  packed.Count[3] = static_cast<float>(ShadowRegionCount_);
  packed.ShadowRegions = ShadowRegions_;
  packed.Environment[3] =
      Reflections_.Atlas != nullptr ? static_cast<float>(IndirectLight.SkyLux) : 0.0f;
  for (size_t at = 0; at < 3; ++at) {
    packed.SkyUp[at] = static_cast<float>(Reflections_.Up[at]);
    packed.SkyToSun[at] = static_cast<float>(Reflections_.ToSun[at]);
  }
  return std::bit_cast<std::array<float, kLightFloats>>(packed);
}

}
