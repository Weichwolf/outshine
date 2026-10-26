#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALBINDING_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALBINDING_H

#include <cstddef>
#include <cstdint>

#include "Material.h"
#include "../SurfaceState.h"
#include "Texture.h"
#include "UvTransform.h"

namespace outshine::Render {

inline constexpr uint32_t kNoMaterialSlot = ~0u;

constexpr uint32_t kMaterialTextureCount = 8;
constexpr uint32_t kGeometryFragmentImageCount = kMaterialTextureCount + 3;
constexpr uint32_t kGeometryFragmentUniformCount = 2;

struct MaterialTexture {
  const uint8_t *Rgba = nullptr;
  uint32_t Width = 0;
  uint32_t Height = 0;
  ImagePixelFormat Format = ImagePixelFormat::Rgba8Unorm;
  ImageMipViews LowerMips{};
  outshine::Wrap WrapU = outshine::Wrap::Repeat;
  outshine::Wrap WrapV = outshine::Wrap::Repeat;
  outshine::Filter Magnify = outshine::Filter::Linear;

  outshine::Filter Minify = outshine::Filter::Linear;
  outshine::MipFilter Mip = outshine::MipFilter::Linear;

  outshine::UvTransform Uv;

  outshine::UvSet Set = outshine::UvSet::Uv0;
};

enum class SurfaceDomain : std::uint8_t { Geometry, Ground };

inline constexpr std::size_t kSurfaceDomains = 2;

struct MaterialBinding {
  Material Row;
  SurfaceDomain Domain = SurfaceDomain::Geometry;
  MaterialTexture Colour;

  MaterialTexture SpecularStrength;
  MaterialTexture SpecularTint;
  MaterialTexture DiffuseReflection;
  MaterialTexture DiffuseTransmission;

  MaterialTexture Normal;
  MaterialTexture MetalRough;
  MaterialTexture Emissive;

  float NormalScale = 1.0f;

  [[nodiscard]] SurfaceState State() const { return StateOf(Row); }

  [[nodiscard]] float Coverage() const { return Row.BaseColour[3]; }

  [[nodiscard]] bool ReadsSecondUv() const {
    return Row.Pattern == SurfacePattern::FacadeMetres || Colour.Set == outshine::UvSet::Uv1 ||
           Normal.Set == outshine::UvSet::Uv1 || MetalRough.Set == outshine::UvSet::Uv1 ||
           Emissive.Set == outshine::UvSet::Uv1 || SpecularStrength.Set == outshine::UvSet::Uv1 ||
           SpecularTint.Set == outshine::UvSet::Uv1 ||
           DiffuseReflection.Set == outshine::UvSet::Uv1 ||
           DiffuseTransmission.Set == outshine::UvSet::Uv1;
  }

  [[nodiscard]] bool ReadsAnyImage() const {
    return (Colour.Rgba != nullptr) || (Normal.Rgba != nullptr) || (MetalRough.Rgba != nullptr) ||
           (Emissive.Rgba != nullptr) || (SpecularStrength.Rgba != nullptr) ||
           (SpecularTint.Rgba != nullptr) || (DiffuseReflection.Rgba != nullptr) ||
           (DiffuseTransmission.Rgba != nullptr);
  }
};

}

#endif
