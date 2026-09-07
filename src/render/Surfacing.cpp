#include "Surfacing.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>

namespace outshine::Render {

namespace Says {
constexpr auto MissingNativeImage = "native material names absent image {}";
}

namespace {
SubjectWrap WrapOf(Wrap wrap) {
  switch (wrap) {
    case Wrap::ClampToEdge: return SubjectWrap::ClampToEdge;
    case Wrap::MirroredRepeat: return SubjectWrap::MirroredRepeat;
    case Wrap::Repeat: return SubjectWrap::Repeat;
  }
  return SubjectWrap::Repeat;
}

SubjectMip MipOf(MipFilter mip) {
  switch (mip) {
    case MipFilter::None: return SubjectMip::None;
    case MipFilter::Nearest: return SubjectMip::Nearest;
    case MipFilter::Linear: return SubjectMip::Linear;
  }
  return SubjectMip::Linear;
}
} // namespace

bool ResolveNativeTextures(const Geometry &geometry,
                           std::span<SubjectMaterial> surfaces,
                           std::string &error) {
  std::vector<ImageView> images;
  images.reserve(static_cast<size_t>(geometry.images()));
  for (int at = 0; at < geometry.images(); ++at) { images.push_back(geometry.imageAt(at)); }
  return ResolveNativeTextures(images, surfaces, error);
}

bool ResolveNativeTextures(std::span<const ImageView> images,
                           std::span<SubjectMaterial> surfaces,
                           std::string &error) {
  const auto bind = [images, &error](const SurfaceMap &map, SubjectTexture &texture) {
    texture = {};
    if (!map.bound()) { return true; }
    const ImageView image = static_cast<size_t>(map.Image) < images.size()
                                ? images[static_cast<size_t>(map.Image)]
                                : ImageView{};
    if (!image.stands()) {
      error = std::format(Says::MissingNativeImage, map.Image);
      return false;
    }
    texture.Rgba = image.Rgba.data();
    texture.Width = static_cast<uint32_t>(image.WidthPx);
    texture.Height = static_cast<uint32_t>(image.HeightPx);
    texture.Set = map.Set;
    texture.Uv = UvTransformOf(map.Uv);
    texture.WrapU = WrapOf(map.Sampler.WrapU);
    texture.WrapV = WrapOf(map.Sampler.WrapV);
    texture.Magnify =
        map.Sampler.Magnify == Filter::Nearest ? SubjectFilter::Nearest : SubjectFilter::Linear;
    texture.Minify =
        map.Sampler.Minify == Filter::Nearest ? SubjectFilter::Nearest : SubjectFilter::Linear;
    texture.Mip = MipOf(map.Sampler.Mip);
    return true;
  };
  for (SubjectMaterial &surface : surfaces) {
    const Material &row = surface.Row;
    if (!bind(row.BaseColourMap, surface.Colour) || !bind(row.NormalMap, surface.Normal) ||
        !bind(row.MetalRoughMap, surface.MetalRough) || !bind(row.EmissiveMap, surface.Emissive) ||
        !bind(row.SpecularStrengthMap, surface.SpecularStrength) ||
        !bind(row.SpecularTintMap, surface.SpecularTint)) {
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
  out.PartSlot.clear();
  out.Decoded.clear();

  out.PartSlot.assign(geometry.Parts.size(), 0u);
  if (geometry.Surfaces.empty()) {
    SubjectMaterial slot;
    slot.Row = row;
    out.Slots.push_back(slot);
    out.Decoded.emplace_back();
    out.Material.push_back(0);
    return;
  }
  for (size_t part = 0; part < geometry.Parts.size(); ++part) {
    const int material = geometry.Parts[part].Material;
    size_t slot = out.Material.size();
    for (size_t at = 0; at < out.Material.size(); ++at) {
      if (out.Material[at] == material) {
        slot = at;
        break;
      }
    }
    if (slot == out.Material.size()) {
      SubjectMaterial surface;
      surface.Row = material >= 0 && static_cast<size_t>(material) < geometry.Surfaces.size()
                        ? geometry.Surfaces[static_cast<size_t>(material)]
                        : row;
      out.Slots.push_back(surface);
      out.Decoded.emplace_back();
      out.Material.push_back(material);
    }
    out.PartSlot[part] = static_cast<uint32_t>(slot);
  }
  for (size_t surface = 0; surface < geometry.Surfaces.size(); ++surface) {
    if (std::ranges::find(out.Material, static_cast<int>(surface)) != out.Material.end()) {
      continue;
    }
    SubjectMaterial unworn;
    unworn.Row = geometry.Surfaces[surface];
    out.Slots.push_back(unworn);
    out.Decoded.emplace_back();
    out.Material.push_back(static_cast<int>(surface));
  }
}

} // namespace outshine::Render
