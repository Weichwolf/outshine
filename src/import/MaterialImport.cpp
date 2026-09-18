#include "MaterialImport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Document.h"
#include "Image.h"
#include "Types.h"

namespace outshine::Gltf {
namespace {

struct NativeSocket {
  const TextureRef &Declared;
  SurfaceMap outshine::Material::*Destination;
  const char *Name;
};

[[nodiscard]] outshine::Filter NativeFilter(Filter filter) noexcept {
  return filter == Filter::Nearest ? outshine::Filter::Nearest : outshine::Filter::Linear;
}

[[nodiscard]] outshine::MipFilter NativeMip(MipFilter filter) noexcept {
  switch (filter) {
    case MipFilter::None: return outshine::MipFilter::None;
    case MipFilter::Nearest: return outshine::MipFilter::Nearest;
    case MipFilter::Linear: return outshine::MipFilter::Linear;
  }
  return outshine::MipFilter::Linear;
}

[[nodiscard]] outshine::Wrap NativeWrap(Wrap wrap) noexcept {
  switch (wrap) {
    case Wrap::ClampToEdge: return outshine::Wrap::ClampToEdge;
    case Wrap::MirroredRepeat: return outshine::Wrap::MirroredRepeat;
    case Wrap::Repeat: return outshine::Wrap::Repeat;
  }
  return outshine::Wrap::Repeat;
}

[[nodiscard]] std::expected<int, std::string> KeepImage(const Document &document,
                                                        const Material &material,
                                                        const TextureRef &declared,
                                                        const char *socket,
                                                        std::vector<Core::Raster> &images) {
  const Texture &texture = document.Textures()[static_cast<size_t>(declared.Texture)];
  std::vector<uint8_t> encoded;
  if (!document.ImageBytes(texture.Source, encoded)) {
    return std::unexpected("material '" + material.Name + "' names " + socket + " image " +
                           std::to_string(texture.Source) + ", whose bytes could not be read");
  }
  Core::Raster raster;
  if (!DecodeImage(encoded.data(), encoded.size(), raster) || !raster.Holds()) {
    return std::unexpected("the " + std::string(socket) + " image of material '" + material.Name +
                           "' is " + std::to_string(encoded.size()) +
                           " bytes that this decoder does not read");
  }
  const std::span<const uint8_t> pixels = raster.Rgba;
  for (size_t at = 0; at < images.size(); ++at) {
    const Core::Raster &held = images[at];
    if (held.Width == raster.Width && held.Height == raster.Height &&
        held.Rgba.size() == pixels.size() &&
        std::memcmp(held.Rgba.data(), pixels.data(), pixels.size()) == 0) {
      return static_cast<int>(at);
    }
  }
  images.push_back(std::move(raster));
  return static_cast<int>(images.size() - 1);
}

[[nodiscard]] bool ResolveSocket(const Document &document,
                                 const Material &material,
                                 const NativeSocket &socket,
                                 std::vector<Core::Raster> &images,
                                 SurfaceMap &out,
                                 std::string &error) {
  if (!socket.Declared.Declared()) { return true; }
  UvSet set = UvSet::Uv0;
  std::string why;
  if (!UvSetOf(socket.Declared, CarriedUvSets::Both, socket.Name, set, why)) {
    error = "material '" + material.Name + "' " + why;
    return false;
  }
  auto image = KeepImage(document, material, socket.Declared, socket.Name, images);
  if (!image) {
    error = std::move(image.error());
    return false;
  }
  out.Image = *image;
  out.Set = set;
  out.Uv = socket.Declared.Uv;
  const Texture &texture = document.Textures()[static_cast<size_t>(socket.Declared.Texture)];
  if (texture.Sampler >= 0) {
    const Sampler &sampler = document.Samplers()[static_cast<size_t>(texture.Sampler)];
    out.Sampler.Magnify = NativeFilter(sampler.Mag);
    out.Sampler.Minify = NativeFilter(sampler.Min);
    out.Sampler.Mip = NativeMip(sampler.Mip);
    out.Sampler.WrapU = NativeWrap(sampler.WrapS);
    out.Sampler.WrapV = NativeWrap(sampler.WrapT);
  }
  return true;
}

}

void ImportMaterialAssets(const Document &document, MaterialAssetSet &out) {
  std::vector<Core::Raster> images;
  std::vector<MaterialAsset> materials;
  materials.reserve(document.Materials().size());
  for (const Gltf::Material &declared : document.Materials()) {
    MaterialAsset asset{.Name = declared.Name, .Surface = declared.Surface, .Error = {}};
    asset.Surface.NeedsTangents = declared.Normal.Texture >= 0;
    asset.Surface.NormalScale = static_cast<float>(declared.NormalScale);
    asset.Surface.OcclusionStrength = static_cast<float>(declared.OcclusionStrength);
    const std::array<NativeSocket, 7> sockets = {{
        {.Declared = declared.BaseColour,
         .Destination = &outshine::Material::BaseColourMap,
         .Name = "baseColorTexture"},
        {.Declared = declared.Normal,
         .Destination = &outshine::Material::NormalMap,
         .Name = "normalTexture"},
        {.Declared = declared.MetallicRoughness,
         .Destination = &outshine::Material::MetalRoughMap,
         .Name = "metallicRoughnessTexture"},
        {.Declared = declared.Emissive,
         .Destination = &outshine::Material::EmissiveMap,
         .Name = "emissiveTexture"},
        {.Declared = declared.Occlusion,
         .Destination = &outshine::Material::OcclusionMap,
         .Name = "occlusionTexture"},
        {.Declared = declared.SpecularStrength,
         .Destination = &outshine::Material::SpecularStrengthMap,
         .Name = "specularTexture"},
        {.Declared = declared.SpecularTint,
         .Destination = &outshine::Material::SpecularTintMap,
         .Name = "specularColorTexture"},
    }};
    for (const NativeSocket &socket : sockets) {
      if (!ResolveSocket(
              document, declared, socket, images, asset.Surface.*socket.Destination, asset.Error)) {
        break;
      }
    }
    materials.push_back(std::move(asset));
  }
  out.Adopt(std::move(images), std::move(materials));
}

}
