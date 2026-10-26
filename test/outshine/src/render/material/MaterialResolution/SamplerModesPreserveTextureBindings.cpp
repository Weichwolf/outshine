#include "MaterialResolution.h"
#include "MaterialBinding.h"
#include "Material.h"
#include "Texture.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr std::array wraps{Wrap::ClampToEdge, Wrap::MirroredRepeat, Wrap::Repeat};
  constexpr std::array filters{Filter::Nearest, Filter::Linear};
  constexpr std::array mips{MipFilter::None, MipFilter::Nearest, MipFilter::Linear};
  constexpr std::array<std::uint8_t, 4> pixel{30, 60, 90, 255};
  const std::array images{ImageView{.WidthPx = 1, .HeightPx = 1, .Rgba = pixel}};
  for (std::size_t combination = 0; combination < 108; ++combination) {
    const Sampler expected{.Magnify = filters[combination / 9 % 2],
                           .Minify = filters[combination / 18 % 2],
                           .Mip = mips[combination / 36 % 3],
                           .WrapU = wraps[combination % 3],
                           .WrapV = wraps[combination / 3 % 3]};
    std::array<MaterialBinding, 1> bindings;
    Material &row = bindings[0].Row;
    const std::array maps{&row.BaseColourMap,
                          &row.NormalMap,
                          &row.MetalRoughMap,
                          &row.EmissiveMap,
                          &row.SpecularStrengthMap,
                          &row.SpecularTintMap,
                          &row.DiffuseReflectionMap,
                          &row.DiffuseTransmissionMap};
    for (SurfaceMap *map : maps) {
      map->Image = 0;
      map->Sampler = expected;
    }
    std::string error;
    const bool resolved = ResolveNativeTextures(images, bindings, error);
    CHECK(resolved, "all declared sampler combinations resolve");
    if (!resolved) { continue; }
    const MaterialBinding &binding = bindings[0];
    const std::array textures{&binding.Colour,
                              &binding.Normal,
                              &binding.MetalRough,
                              &binding.Emissive,
                              &binding.SpecularStrength,
                              &binding.SpecularTint,
                              &binding.DiffuseReflection,
                              &binding.DiffuseTransmission};
    for (const MaterialTexture *texture : textures) {
      CHECK(texture->WrapU == expected.WrapU && texture->WrapV == expected.WrapV &&
                texture->Magnify == expected.Magnify && texture->Minify == expected.Minify &&
                texture->Mip == expected.Mip && texture->Rgba == pixel.data(),
            "every material channel preserves source sampling and borrowed image storage");
    }
  }
  for (unsigned value = 2; value < 256; ++value) {
    std::array<MaterialBinding, 1> bindings;
    SurfaceMap &map = bindings[0].Row.BaseColourMap;
    map.Image = 0;
    map.Sampler = {.Magnify = static_cast<Filter>(value),
                   .Minify = static_cast<Filter>(value),
                   .Mip = static_cast<MipFilter>(value),
                   .WrapU = static_cast<Wrap>(value),
                   .WrapV = static_cast<Wrap>(value)};
    std::string error;
    CHECK(ResolveNativeTextures(images, bindings, error),
          "unknown sampler values retain the existing fallback policy");
    const MaterialTexture &texture = bindings[0].Colour;
    CHECK(texture.WrapU == Wrap::Repeat && texture.WrapV == Wrap::Repeat &&
              texture.Magnify == Filter::Linear && texture.Minify == Filter::Linear &&
              texture.Mip == MipFilter::Linear,
          "unknown modes normalize before residency keys and device creation");
  }
  return Report();
}
