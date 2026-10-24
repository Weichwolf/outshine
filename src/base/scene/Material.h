#ifndef OUTSHINE_BASE_SCENE_MATERIAL_H
#define OUTSHINE_BASE_SCENE_MATERIAL_H

#include <limits>

#include "Vec4.h"
#include "Vec3.h"
#include "Texture.h"

namespace outshine {

constexpr float kIorUnsaid = 1.5f;

constexpr float kIridescenceIorUnsaid = 1.3f;

constexpr float kIridescenceThicknessMaxUnsaidNm = 400.0f;

enum class AlphaMode { Opaque, Masked, Blended };

enum class SurfacePattern { None, Facade, FacadeMetres };

enum class ScatteringModel { MetallicRoughness, DiffuseThinSheet, AreaMixture };

struct Material {

  ScatteringModel Scattering = ScatteringModel::MetallicRoughness;

  Vec3f DiffuseTransmission;

  SurfacePattern Pattern = SurfacePattern::None;

  Vec4f BaseColour = {{0.5f, 0.5f, 0.5f, 1.0f}};

  float Metalness = 0.0f;

  float Roughness = 1.0f;

  float Transmission = 0.0f;

  float Ior = kIorUnsaid;

  Vec3f Emission;

  AlphaMode Alpha = AlphaMode::Opaque;

  bool DoubleSided = false;

  float CoverageCut = 0.5f;

  bool Unlit = false;

  bool NeedsTangents = false;

  float NormalScale = 1.0f;

  float OcclusionStrength = 1.0f;

  float SpecularFactor = 1.0f;

  Vec3f SpecularColour = {{1.0f, 1.0f, 1.0f}};

  Vec3f SheenColour;

  float SheenRoughness = 0.0f;

  float Clearcoat = 0.0f;

  float ClearcoatRoughness = 0.0f;

  float Anisotropy = 0.0f;

  float AnisotropyRotationRad = 0.0f;

  float Iridescence = 0.0f;

  float IridescenceIor = kIridescenceIorUnsaid;

  float IridescenceThicknessMinNm = 100.0f;

  float IridescenceThicknessMaxNm = kIridescenceThicknessMaxUnsaidNm;

  float Thickness = 0.0f;

  float AttenuationDistance = std::numeric_limits<float>::infinity();

  Vec3f AttenuationColour = {{1.0f, 1.0f, 1.0f}};

  SurfaceMap BaseColourMap;

  SurfaceMap NormalMap;

  SurfaceMap MetalRoughMap;

  SurfaceMap EmissiveMap;

  SurfaceMap OcclusionMap;

  SurfaceMap SpecularStrengthMap;

  SurfaceMap SpecularTintMap;

  SurfaceMap DiffuseReflectionMap;

  SurfaceMap DiffuseTransmissionMap;

  [[nodiscard]] bool operator==(const Material &) const = default;
};

constexpr void DielectricF0(const Material &material, Vec3f &out) noexcept {
  if (material.Ior == 0.0f) {
    out[0] = out[1] = out[2] = 0.0f;
    return;
  }
  const float edge = (material.Ior - 1.0f) / (material.Ior + 1.0f);
  const float scaled = edge * edge * material.SpecularFactor;
  const float capped = scaled < 1.0f ? scaled : 1.0f;
  for (int channel = 0; channel < 3; ++channel) {
    out[channel] = material.SpecularColour[channel] * capped;
  }
}

[[nodiscard]] constexpr float DielectricF90(const Material &material) noexcept {
  return material.Ior == 0.0f ? 0.0f : material.SpecularFactor;
}

class MaterialInstance {
public:
  constexpr MaterialInstance() noexcept = default;

  constexpr explicit MaterialInstance(int at) noexcept : At_(at) {}

  [[nodiscard]] constexpr bool bound() const noexcept { return At_ >= 0; }

  [[nodiscard]] constexpr int index() const noexcept { return At_; }

  [[nodiscard]] constexpr bool operator==(const MaterialInstance &) const noexcept = default;

private:
  int At_ = -1;
};

constexpr int kMaterialRowFloats = 20;

}
#endif
