#ifndef OUTSHINE_CONTENT_SHADE_MATERIAL_H
#define OUTSHINE_CONTENT_SHADE_MATERIAL_H

#include <limits>

namespace outshine {

enum class AlphaMode { Opaque, Masked, Blended };

struct Material {

  float BaseColour[4] = {0.5f, 0.5f, 0.5f, 1.0f};
  float Metalness = 0.0f;
  float Roughness = 1.0f;
  float Transmission = 0.0f;
  float Ior = 1.5f;
  float Emission[3] = {0.0f, 0.0f, 0.0f};
  AlphaMode Alpha = AlphaMode::Opaque;

  bool DoubleSided = false;

  float CoverageCut = 0.5f;

  bool Unlit = false;

  float SpecularFactor = 1.0f;
  float SpecularColour[3] = {1.0f, 1.0f, 1.0f};

  float SheenColour[3] = {0.0f, 0.0f, 0.0f};
  float SheenRoughness = 0.0f;

  float Clearcoat = 0.0f;
  float ClearcoatRoughness = 0.0f;

  float Anisotropy = 0.0f;
  float AnisotropyRotationRad = 0.0f;

  float Iridescence = 0.0f;
  float IridescenceIor = 1.3f;
  float IridescenceThicknessMinNm = 100.0f;
  float IridescenceThicknessMaxNm = 400.0f;
  float Thickness = 0.0f;
  float AttenuationDistance = std::numeric_limits<float>::infinity();
  float AttenuationColour[3] = {1.0f, 1.0f, 1.0f};
};

inline void DielectricF0(const Material &material, float out[3]) {
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

inline float DielectricF90(const Material &material) {
  return material.Ior == 0.0f ? 0.0f : material.SpecularFactor;
}

constexpr int kMaterialRowFloats = 20;

}
#endif
