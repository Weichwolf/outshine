#ifndef OUTSHINE_MATERIAL_H
#define OUTSHINE_MATERIAL_H

#include <limits>

#include "math/Vec4.h"
#include "math/Vec3.h"
#include "Texture.h"

namespace outshine {

/// The index of refraction a dielectric stands at when nothing declares one -- ordinary glass.
constexpr float kIorUnsaid = 1.5f;

/// The index of refraction of the thin film an iridescent surface carries.
constexpr float kIridescenceIorUnsaid = 1.3f;

/// How thick that film may become, in nanometres.
constexpr float kIridescenceThicknessMaxUnsaidNm = 400.0f;

enum class AlphaMode { Opaque, Masked, Blended };

struct Material {
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
};

inline void DielectricF0(const Material &material, Vec3f &out) {
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

class MaterialInstance {
public:
  MaterialInstance() = default;

  explicit MaterialInstance(int at) : At_(at) {}

  [[nodiscard]] bool bound() const { return At_ >= 0; }

  [[nodiscard]] int index() const { return At_; }

  [[nodiscard]] bool operator==(const MaterialInstance &) const = default;

private:
  int At_ = -1;
};

constexpr int kMaterialRowFloats = 20;

} // namespace outshine
#endif
