#ifndef OUTSHINE_MATERIAL_H
#define OUTSHINE_MATERIAL_H

#include <limits>

#include "math/Vec4.h"
#include "math/Vec3.h"
#include "Texture.h"

namespace outshine {

/// Default dimensionless dielectric index of refraction.
constexpr float kIorUnsaid = 1.5f;

/// The index of refraction of the thin film an iridescent surface carries.
constexpr float kIridescenceIorUnsaid = 1.3f;

/// How thick that film may become, in nanometres.
constexpr float kIridescenceThicknessMaxUnsaidNm = 400.0f;

/// Coverage interpretation independent of transmission through the material.
enum class AlphaMode {
  Opaque, ///< Fully covered surface; ignore base-colour alpha for coverage.
  Masked, ///< Discard fragments whose base-colour alpha is below CoverageCut.
  Blended ///< Blend using straight base-colour alpha.
};

/// Native metallic-roughness surface parameters; default is a grey dielectric.
/// Factors are dimensionless unless a field specifies units. Supply finite values in the
/// documented ranges; this value type performs no validation or clamping. The only permitted
/// infinity is positive AttenuationDistance. Declaring a feature does not establish renderer
/// support or an accuracy guarantee for that feature.
/// Copies own their factors, but image indices borrow the containing Geometry's image table.
/// Remap indices when copying to another geometry; they retain no image or owner lifetime.
/// No thread affinity or internal synchronization; immutable reads may run concurrently,
/// mutations require external synchronization. Copying allocates nothing.
struct Material {
  /// Linear RGB reflectance and straight coverage alpha, each in [0,1]; multiplies BaseColourMap.
  Vec4f BaseColour = {{0.5f, 0.5f, 0.5f, 1.0f}};
  /// Metallic fraction in [0,1]; multiplies the blue channel of MetalRoughMap.
  float Metalness = 0.0f;
  /// Perceptual roughness in [0,1]; multiplies the green channel of MetalRoughMap.
  float Roughness = 1.0f;
  /// Fraction of light transmitted through the surface, in [0,1]; separate from coverage alpha.
  float Transmission = 0.0f;
  /// Dimensionless index of refraction, at least 1; zero explicitly disables dielectric Fresnel.
  float Ior = kIorUnsaid;
  /// Nonnegative linear RGB emission factor; may exceed 1 and multiplies EmissiveMap.
  Vec3f Emission;
  /// Coverage mode; use a supported AlphaMode value.
  AlphaMode Alpha = AlphaMode::Opaque;

  /// Render both faces; shading must orient the back-face frame consistently.
  bool DoubleSided = false;

  /// Nonnegative alpha threshold used only by Masked coverage; values above 1 discard all.
  float CoverageCut = 0.5f;

  /// Shade using base colour without incident-light BRDF evaluation.
  bool Unlit = false;

  /// Preparation hint requesting a tangent basis; does not supply tangent data itself.
  bool NeedsTangents = false;

  /// Dielectric specular strength in [0,1]; multiplies SpecularStrengthMap alpha.
  float SpecularFactor = 1.0f;
  /// Linear RGB dielectric specular tint in [0,1]; multiplies SpecularTintMap RGB.
  Vec3f SpecularColour = {{1.0f, 1.0f, 1.0f}};

  /// Linear RGB sheen-layer reflectance in [0,1]; zero disables sheen.
  Vec3f SheenColour;
  /// Perceptual sheen-layer roughness in [0,1].
  float SheenRoughness = 0.0f;

  /// Clearcoat-layer strength in [0,1]; zero disables the layer.
  float Clearcoat = 0.0f;
  /// Perceptual clearcoat-layer roughness in [0,1].
  float ClearcoatRoughness = 0.0f;

  /// Anisotropy strength in [0,1]; zero selects isotropic reflection.
  float Anisotropy = 0.0f;
  /// Rotation of the anisotropic direction from tangent toward bitangent, in radians.
  float AnisotropyRotationRad = 0.0f;

  /// Thin-film interference strength in [0,1]; zero disables the effect.
  float Iridescence = 0.0f;
  /// Dimensionless thin-film index of refraction, at least 1.
  float IridescenceIor = kIridescenceIorUnsaid;
  /// Nonnegative minimum film thickness in nanometres; no greater than the maximum.
  float IridescenceThicknessMinNm = 100.0f;
  /// Nonnegative maximum film thickness in nanometres; current shading uses this endpoint.
  float IridescenceThicknessMaxNm = kIridescenceThicknessMaxUnsaidNm;
  /// Nonnegative nominal transmission path length in metres; current shading uses it directly.
  float Thickness = 0.0f;
  /// Positive distance in metres at which AttenuationColour is reached; +infinity disables
  /// absorption.
  float AttenuationDistance = std::numeric_limits<float>::infinity();
  /// Linear RGB transmittance in [0,1] after AttenuationDistance metres.
  Vec3f AttenuationColour = {{1.0f, 1.0f, 1.0f}};

  /// Base-colour RGB sampled as sRGB and converted to linear; alpha remains linear coverage.
  SurfaceMap BaseColourMap;
  /// Linear tangent-space normal RGB encoded from [-1,1] to [0,1]; requires a valid tangent frame.
  SurfaceMap NormalMap;
  /// Linear data texture: green roughness, blue metalness; other channels unused by this binding.
  SurfaceMap MetalRoughMap;
  /// Emissive RGB sampled as sRGB and converted to linear; multiplied by Emission.
  SurfaceMap EmissiveMap;
  /// Linear red-channel ambient occlusion factor; one means unoccluded.
  SurfaceMap OcclusionMap;
  /// Linear alpha-channel multiplier of SpecularFactor.
  SurfaceMap SpecularStrengthMap;
  /// sRGB RGB multiplier of SpecularColour, converted to linear before use.
  SurfaceMap SpecularTintMap;

  /// Compare stored factors and bindings exactly; no image-content comparison or allocation.
  /// @return Whether every stored value matches; NaN values compare unequal.
  [[nodiscard]] bool operator==(const Material &) const = default;
};

/// Compute normal-incidence dielectric Fresnel reflectance; does not blend in metallic response.
/// @param material Valid Ior, SpecularFactor and SpecularColour as documented by Material.
/// @param out Caller-owned linear RGB reflectance, overwritten in full; may alias SpecularColour.
/// Constant work, no allocation or mutation of other material fields. Zero Ior produces zero.
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

/// Compute the grazing-angle dielectric specular factor without allocation or mutation.
/// @param material Valid Ior and SpecularFactor as documented by Material.
/// @return Zero for disabled Fresnel (Ior zero), otherwise SpecularFactor; constant work.
[[nodiscard]] constexpr float DielectricF90(const Material &material) noexcept {
  return material.Ior == 0.0f ? 0.0f : material.SpecularFactor;
}

/// Non-owning index into one Geometry's material table, not a persistent entity/resource handle.
/// No owner identity or generation is stored; callers must retain the original owner and remap
/// when crossing geometries. Clearing/destroying that owner invalidates the reference, and an
/// index may later be reused. Value operations allocate nothing and have no thread affinity;
/// synchronize writes to shared instances externally.
class MaterialInstance {
public:
  /// Construct an unbound reference.
  constexpr MaterialInstance() noexcept = default;

  /// Store an owner-local index without validating an owner or table size.
  /// @param at Nonnegative index, or any negative value for an unbound reference.
  constexpr explicit MaterialInstance(int at) noexcept : At_(at) {}

  /// @return Whether an index is assigned; does not establish that the material still exists.
  [[nodiscard]] constexpr bool bound() const noexcept { return At_ >= 0; }

  /// @return Stored index, including the original negative value when unbound.
  [[nodiscard]] constexpr int index() const noexcept { return At_; }

  /// @return Equality of stored indices only; equal values need not have the same owner.
  [[nodiscard]] constexpr bool operator==(const MaterialInstance &) const noexcept = default;

private:
  int At_ = -1;
};

/// Float count of the legacy packed generator material row; not the layout of Material.
constexpr int kMaterialRowFloats = 20;

}
#endif
