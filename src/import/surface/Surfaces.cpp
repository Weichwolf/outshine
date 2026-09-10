#include "Surfaces.h"

#include "Image.h"
#include "Variant.h"
#include <array>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <string>
#include <cstddef>
#include <vector>
#include <cstdint>

namespace outshine::Gltf {
namespace Says {
constexpr auto InconsistentSlots = "surface table has inconsistent slot references";
}

namespace {

Render::SubjectMip MipOf(MipFilter mip) {
  switch (mip) {
    case MipFilter::None: return Render::SubjectMip::None;
    case MipFilter::Nearest: return Render::SubjectMip::Nearest;
    case MipFilter::Linear: return Render::SubjectMip::Linear;
  }
  return Render::SubjectMip::Linear;
}

Render::SubjectWrap WrapOf(Wrap wrap) {
  switch (wrap) {
    case Wrap::ClampToEdge: return Render::SubjectWrap::ClampToEdge;
    case Wrap::MirroredRepeat: return Render::SubjectWrap::MirroredRepeat;
    case Wrap::Repeat: return Render::SubjectWrap::Repeat;
  }
  return Render::SubjectWrap::Repeat;
}

[[nodiscard]] bool ReadSocketImage(const Document &file,
                                   const Material &material,
                                   const TextureRef &declared,
                                   const char *socket,
                                   CarriedUvSets carried,
                                   Core::Raster &raster,
                                   Render::SubjectTexture &bound,
                                   std::string &error) {
  if (!declared.Declared()) { return true; }

  std::string why;
  if (!UvSetOf(declared, carried, socket, bound.Set, why)) {
    error = std::string("material '") + material.Name + "' " + why;
    return false;
  }
  const Texture &texture = file.Textures()[static_cast<size_t>(declared.Texture)];
  std::vector<uint8_t> encoded;
  if (!file.ImageBytes(texture.Source, encoded)) {
    error = std::string("material '") + material.Name + "' names " + socket + " image " +
            std::to_string(texture.Source) + ", whose bytes could not be read";
    return false;
  }
  if (!DecodeImage(encoded.data(), encoded.size(), raster) || !raster.Holds()) {
    error = std::string("the ") + socket + " image of material '" + material.Name + "' is " +
            std::to_string(encoded.size()) + " bytes that this decoder does not read";
    return false;
  }
  bound.Rgba = raster.Rgba.data();
  bound.Width = static_cast<uint32_t>(raster.Width);
  bound.Height = static_cast<uint32_t>(raster.Height);

  bound.Uv = UvTransformOf(declared.Uv);
  if (texture.Sampler >= 0) {
    const Sampler &sampler = file.Samplers()[static_cast<size_t>(texture.Sampler)];
    bound.WrapU = WrapOf(sampler.WrapS);
    bound.WrapV = WrapOf(sampler.WrapT);
    bound.Magnify = sampler.Mag == Filter::Nearest ? Render::SubjectFilter::Nearest
                                                   : Render::SubjectFilter::Linear;
    bound.Minify = sampler.Min == Filter::Nearest ? Render::SubjectFilter::Nearest
                                                  : Render::SubjectFilter::Linear;
    bound.Mip = MipOf(sampler.Mip);
  }
  return true;
}

}

void ResolveSurfaceTable([[maybe_unused]] const Document &file,
                         const Subject &geometry,
                         bool carriesTransmission,
                         bool ownMaterials,
                         Render::SurfaceTable &out) {
  out.Slots.clear();
  out.Material.clear();
  out.Decoded.clear();
  out.PartSlot.assign(geometry.Parts().size(), 0);
  for (size_t part = 0; part < geometry.Parts().size(); ++part) {
    const int material = geometry.Parts()[part].Material;
    size_t slot = out.Material.size();
    for (size_t at = 0; at < out.Material.size(); ++at) {
      if (out.Material[at] == material) {
        slot = at;
        break;
      }
    }
    if (slot == out.Material.size()) {
      Render::SubjectMaterial surface;
      surface.Row = DefaultMaterial();
      if (material >= 0 && static_cast<size_t>(material) < geometry.Surfaces().size()) {
        surface.Row = geometry.Surfaces()[static_cast<size_t>(material)];
        if (!carriesTransmission) {
          surface.Row.Transmission = 0.0f;
          surface.Row.Thickness = 0.0f;
        }

        if (!ownMaterials) { surface.Row.Alpha = AlphaMode::Opaque; }
      }
      out.Material.push_back(material);
      out.Slots.push_back(surface);
    }
    out.PartSlot[part] = static_cast<uint32_t>(slot);
  }
}

namespace {

[[nodiscard]] bool ReadMaterialMaps(const Document &file,
                                    CarriedUvSets carried,
                                    Render::SurfaceTable &table,
                                    std::string &error) {
  for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
    const int index = table.Material[slot];
    if (index < 0 || static_cast<size_t>(index) >= file.Materials().size()) { continue; }
    const Material &material = file.Materials()[static_cast<size_t>(index)];
    table.Slots[slot].NormalScale = static_cast<float>(material.NormalScale);

    struct MapRow {
      const TextureRef &Declared;
      const char *Socket;
      Core::Raster &Into;
      Render::SubjectTexture &Bound;
    };

    const std::array<MapRow, 5> maps = {{
        {.Declared = material.Normal,
         .Socket = "normalTexture",
         .Into = table.Decoded[slot].Normal,
         .Bound = table.Slots[slot].Normal},
        {.Declared = material.MetallicRoughness,
         .Socket = "metallicRoughnessTexture",
         .Into = table.Decoded[slot].MetalRough,
         .Bound = table.Slots[slot].MetalRough},
        {.Declared = material.Emissive,
         .Socket = "emissiveTexture",
         .Into = table.Decoded[slot].Emissive,
         .Bound = table.Slots[slot].Emissive},
        {.Declared = material.SpecularStrength,
         .Socket = "specularTexture",
         .Into = table.Decoded[slot].SpecularStrength,
         .Bound = table.Slots[slot].SpecularStrength},
        {.Declared = material.SpecularTint,
         .Socket = "specularColorTexture",
         .Into = table.Decoded[slot].SpecularTint,
         .Bound = table.Slots[slot].SpecularTint},
    }};

    for (const auto &map : maps) {
      if (!ReadSocketImage(
              file, material, map.Declared, map.Socket, carried, map.Into, map.Bound, error)) {
        return false;
      }
    }
  }
  return true;
}

[[nodiscard]] bool ValidateCarrier(const Subject &geometry,
                                   Render::ColourFrom channel,
                                   Render::ColourCarrier carrier,
                                   size_t textured,
                                   std::string &error) {
  const char *socket =
      channel == Render::ColourFrom::Emissive ? "emissiveTexture" : "baseColorTexture";
  if (channel != Render::ColourFrom::Row && carrier == Render::ColourCarrier::Texture &&
      textured == 0) {
    error = std::string("the declaration hands the surface to the file's ") + socket +
            " and no material of it declares one";
    return false;
  }
  if (carrier != Render::ColourCarrier::Texture && textured > 0) {
    error = std::string("the declaration says the appearance is not the ") + socket +
            " IMAGE and " + std::to_string(textured) +
            " material(s) of the file declare an image on that socket, "
            "which this case would then be sampling instead";
    return false;
  }

  if (carrier == Render::ColourCarrier::VertexColour && !geometry.HasColour()) {
    error =
        "the declaration says the appearance is carried by COLOR_0 and no primitive of the subject "
        "declares one";
    return false;
  }
  if (textured > 0 && !geometry.HasUv()) {
    error = "the file's materials are the surface and the subject carries no TEXCOORD_0 to sample "
            "them with";
    return false;
  }
  return true;
}

[[nodiscard]] bool ResolveTextures(const Document &file,
                                   const Subject &geometry,
                                   Render::ColourFrom channel,
                                   Render::ColourCarrier carrier,
                                   Render::SurfaceTable &table,
                                   std::string &error) {
  table.Decoded.assign(table.Slots.size(), Render::SurfaceRasters{});
  const char *socket =
      channel == Render::ColourFrom::Emissive ? "emissiveTexture" : "baseColorTexture";

  const CarriedUvSets carried = geometry.HasUv1() ? CarriedUvSets::Both : CarriedUvSets::FirstOnly;
  size_t textured = 0;
  for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
    const int index = table.Material[slot];
    if (index < 0 || static_cast<size_t>(index) >= file.Materials().size()) { continue; }
    const Material &material = file.Materials()[static_cast<size_t>(index)];
    const TextureRef &declared =
        channel == Render::ColourFrom::Emissive ? material.Emissive : material.BaseColour;
    if (table.Slots[slot].State().Kind() != SurfaceKind::Opaque &&
        material.BaseColour.Texture != declared.Texture) {
      error = std::string("material '") + material.Name +
              "' is not OPAQUE, takes its colour from " + socket + " " +
              std::to_string(declared.Texture) + " and its coverage from " + "baseColorTexture " +
              std::to_string(material.BaseColour.Texture) +
              ", and this subject binds one image per surface -- the second binding is the missing "
              "capability, not a texture to substitute";
      return false;
    }
    if (!declared.Declared()) { continue; }
    if (!ReadSocketImage(file,
                         material,
                         declared,
                         socket,
                         carried,
                         table.Decoded[slot].Colour,
                         table.Slots[slot].Colour,
                         error)) {
      return false;
    }
    ++textured;
  }

  if (channel == Render::ColourFrom::Row && !ReadMaterialMaps(file, carried, table, error)) {
    return false;
  }

  return ValidateCarrier(geometry, channel, carrier, textured, error);
}

}

[[nodiscard]] bool ResolveFileSurface(const Document &file,
                                      const Subject &geometry,
                                      Render::ColourFrom channel,
                                      Render::ColourCarrier carrier,
                                      Render::SurfaceTable &table,
                                      std::string &error) {
  static_assert(std::is_nothrow_move_assignable_v<Render::SurfaceTable>);
  if (table.Material.size() != table.Slots.size() ||
      std::ranges::any_of(table.PartSlot,
                          [&](uint32_t slot) { return slot >= table.Slots.size(); })) {
    error = Says::InconsistentSlots;
    return false;
  }
  Render::SurfaceTable candidate;
  candidate.Material = table.Material;
  candidate.PartSlot = table.PartSlot;
  candidate.Slots.reserve(table.Slots.size());
  for (const auto &slot : table.Slots) {
    Render::SubjectMaterial fresh;
    fresh.Row = slot.Row;
    fresh.Domain = slot.Domain;
    candidate.Slots.push_back(fresh);
  }
  if (!ResolveTextures(file, geometry, channel, carrier, candidate, error)) { return false; }
  table = std::move(candidate);
  error.clear();
  return true;
}

}
