#include "MaterialResolution.h"
#include <span>
#include <string>
#include <vector>
#include <limits>
#include <unordered_map>
#include <cstddef>
#include <cstdint>
#include <format>

namespace outshine::Render {

namespace Says {
constexpr auto MissingNativeImage = "native material names absent image {}";
}

bool ResolveNativeTextures(const Geometry &geometry,
                           std::span<MaterialBinding> surfaces,
                           std::string &error) {
  std::vector<ImageView> images;
  images.reserve(static_cast<size_t>(geometry.images()));
  for (int at = 0; at < geometry.images(); ++at) { images.push_back(geometry.imageAt(at)); }
  return ResolveNativeTextures(images, surfaces, error);
}

bool ResolveNativeTextures(std::span<const ImageView> images,
                           std::span<MaterialBinding> surfaces,
                           std::string &error) {
  const auto bind = [images, &error](const SurfaceMap &map, MaterialTexture &texture) {
    texture = {};
    if (!map.bound()) { return true; }
    const ImageView image = static_cast<size_t>(map.Image) < images.size()
                                ? images[static_cast<size_t>(map.Image)]
                                : ImageView{};
    if (!image.valid()) {
      error = std::format(Says::MissingNativeImage, map.Image);
      return false;
    }
    texture.Rgba = image.Rgba.data();
    texture.Width = static_cast<uint32_t>(image.WidthPx);
    texture.Height = static_cast<uint32_t>(image.HeightPx);
    texture.Format = image.Format;
    texture.LowerMips = image.LowerMips;
    texture.Set = map.Set;
    texture.Uv = UvTransformOf(map.Uv);
    texture.WrapU = map.Sampler.WrapU <= Wrap::Repeat ? map.Sampler.WrapU : Wrap::Repeat;
    texture.WrapV = map.Sampler.WrapV <= Wrap::Repeat ? map.Sampler.WrapV : Wrap::Repeat;
    texture.Magnify = map.Sampler.Magnify == Filter::Nearest ? Filter::Nearest : Filter::Linear;
    texture.Minify = map.Sampler.Minify == Filter::Nearest ? Filter::Nearest : Filter::Linear;
    texture.Mip = map.Sampler.Mip <= MipFilter::Linear ? map.Sampler.Mip : MipFilter::Linear;
    return true;
  };
  for (MaterialBinding &surface : surfaces) {
    const Material &row = surface.Row;
    surface.NormalScale = row.NormalScale;
    if (!bind(row.BaseColourMap, surface.Colour) || !bind(row.NormalMap, surface.Normal) ||
        !bind(row.MetalRoughMap, surface.MetalRough) || !bind(row.EmissiveMap, surface.Emissive) ||
        !bind(row.SpecularStrengthMap, surface.SpecularStrength) ||
        !bind(row.SpecularTintMap, surface.SpecularTint) ||
        !bind(row.DiffuseReflectionMap, surface.DiffuseReflection) ||
        !bind(row.DiffuseTransmissionMap, surface.DiffuseTransmission)) {
      return false;
    }
  }
  return true;
}

void ResolveDeclaredSurface(const Shape &geometry,
                            const outshine::Material &row,
                            SurfaceTable &out) {
  out.Slots.clear();
  out.Material.clear();
  out.NativeMaterial.clear();
  out.PartSlot.assign(geometry.Parts.size(), 0u);
  if (geometry.Surfaces.empty()) {
    out.Slots.emplace_back().Row = row;
    out.Material.push_back(0);
    out.NativeMaterial.push_back(-1);
    return;
  }
  out.Slots.reserve(geometry.Surfaces.size());
  out.Material.reserve(geometry.Surfaces.size());
  out.NativeMaterial.reserve(geometry.Surfaces.size());
  constexpr size_t missing = std::numeric_limits<size_t>::max();
  std::vector<size_t> declaredSlots(geometry.Surfaces.size(), missing);
  std::unordered_map<int, size_t> foreignSlots;
  const auto append = [&](int material) {
    const size_t slot = out.Slots.size();
    out.Slots.emplace_back().Row =
        material >= 0 && static_cast<size_t>(material) < geometry.Surfaces.size()
            ? geometry.Surfaces[static_cast<size_t>(material)]
            : row;
    out.Material.push_back(material);
    out.NativeMaterial.push_back(-1);
    return slot;
  };
  for (size_t part = 0; part < geometry.Parts.size(); ++part) {
    const int material = geometry.Parts[part].Material;
    size_t slot = missing;
    if (material >= 0 && static_cast<size_t>(material) < declaredSlots.size()) {
      auto &declared = declaredSlots[static_cast<size_t>(material)];
      if (declared == missing) { declared = append(material); }
      slot = declared;
    } else {
      auto [entry, inserted] = foreignSlots.try_emplace(material, missing);
      if (inserted) { entry->second = append(material); }
      slot = entry->second;
    }
    out.PartSlot[part] = static_cast<uint32_t>(slot);
  }
  for (size_t surface = 0; surface < declaredSlots.size(); ++surface) {
    if (declaredSlots[surface] == missing) { append(static_cast<int>(surface)); }
  }
}

}
