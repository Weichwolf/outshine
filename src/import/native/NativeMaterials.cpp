#include "NativeMaterials.h"

#include <algorithm>
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
#include "Subject.h"
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
                                                        Geometry &geometry) {
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
  for (int at = 0; at < geometry.images(); ++at) {
    const ImageView held = geometry.imageAt(at);
    if (held.WidthPx == raster.Width && held.HeightPx == raster.Height &&
        held.Rgba.size() == pixels.size() &&
        std::memcmp(held.Rgba.data(), pixels.data(), pixels.size()) == 0) {
      return at;
    }
  }
  const auto kept = geometry.addImage(raster.Width, raster.Height, pixels);
  if (!kept) { return std::unexpected(std::string(Describe(kept.error()))); }
  return *kept;
}

[[nodiscard]] bool ResolveSocket(const Document &document,
                                 const Material &material,
                                 const NativeSocket &socket,
                                 CarriedUvSets carried,
                                 Geometry &geometry,
                                 SurfaceMap &out,
                                 std::string &error) {
  if (!socket.Declared.Declared()) { return true; }
  UvSet set = UvSet::Uv0;
  std::string why;
  if (!UvSetOf(socket.Declared, carried, socket.Name, set, why)) {
    error = "material '" + material.Name + "' " + why;
    return false;
  }
  auto image = KeepImage(document, material, socket.Declared, socket.Name, geometry);
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

[[nodiscard]] bool MaterialIsUsed(const Geometry &geometry, int material) noexcept {
  for (int part = 0; part < geometry.parts(); ++part) {
    if (geometry.materialOf(part).index() == material) { return true; }
  }
  return false;
}

[[nodiscard]] bool MaterialIsDeferredVariant(const Document &document, int material) noexcept {
  bool variant = false;
  for (const Mesh &mesh : document.Meshes()) {
    for (const Primitive &primitive : mesh.Primitives) {
      if (primitive.Material == material) { return false; }
      variant = variant || std::ranges::find(primitive.VariantMaterials, material) !=
                               primitive.VariantMaterials.end();
    }
  }
  return variant;
}

}

bool ResolveNativeMaterialImages(const Document &document,
                                 const Subject &subject,
                                 Geometry &geometry,
                                 std::string &error) {
  const CarriedUvSets carried = subject.HasUv1() ? CarriedUvSets::Both : CarriedUvSets::FirstOnly;
  const size_t many =
      std::min(static_cast<size_t>(geometry.surfaces()), document.Materials().size());
  for (size_t index = 0; index < many; ++index) {
    const int material = static_cast<int>(index);
    const bool deferred =
        !MaterialIsUsed(geometry, material) && MaterialIsDeferredVariant(document, material);
    const Gltf::Material &declared = document.Materials()[index];
    outshine::Material native = geometry.surfaceAt(MaterialInstance(static_cast<int>(index)));
    native.NormalScale = static_cast<float>(declared.NormalScale);
    native.OcclusionStrength = static_cast<float>(declared.OcclusionStrength);
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
    bool unresolved = false;
    for (const NativeSocket &socket : sockets) {
      if (!ResolveSocket(
              document, declared, socket, carried, geometry, native.*socket.Destination, error)) {
        if (!deferred) { return false; }
        unresolved = true;
        error.clear();
        break;
      }
    }
    if (unresolved) { continue; }
    const auto published = geometry.setSurface(MaterialInstance(static_cast<int>(index)), native);
    if (!published) {
      error = "a decoded glTF material could not be published to native geometry: " +
              std::string(Describe(published.error()));
      return false;
    }
  }
  return true;
}

}
