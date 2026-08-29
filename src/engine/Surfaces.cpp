#include "Surfaces.h"

namespace outshine::Core {
namespace {

Render::SubjectWrap WrapOf(Gltf::Wrap wrap) {
  switch (wrap) {
    case Gltf::Wrap::ClampToEdge: return Render::SubjectWrap::ClampToEdge;
    case Gltf::Wrap::MirroredRepeat: return Render::SubjectWrap::MirroredRepeat;
    case Gltf::Wrap::Repeat: return Render::SubjectWrap::Repeat;
  }
  return Render::SubjectWrap::Repeat;
}

[[nodiscard]] bool ReadSocketImage(const Gltf::Document &file, const Gltf::MaterialRef &material,
                                   const Gltf::TextureRef &declared, const char *socket,
                                   Gltf::CarriedUvSets carried,
                                   Raster &raster,
                                   Render::SubjectTexture &bound, std::string &error) {
  if (!declared.Declared()) { return true; }

  std::string why;
  if (!Gltf::UvSetOf(declared, carried, socket, bound.Set, why)) {
    error = std::string("material '") + material.Name + "' " + why;
    return false;
  }
  const Gltf::Texture &texture = file.Textures()[(size_t)declared.Texture];
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
  bound.Width = (uint32_t)raster.Width;
  bound.Height = (uint32_t)raster.Height;

  bound.Uv = declared.Transform;
  if (texture.Sampler >= 0) {
    const Gltf::Sampler &sampler = file.Samplers()[(size_t)texture.Sampler];
    bound.WrapU = WrapOf(sampler.WrapS);
    bound.WrapV = WrapOf(sampler.WrapT);
    bound.Magnify = sampler.Mag == Gltf::Filter::Nearest
        ? Render::SubjectFilter::Nearest
        : Render::SubjectFilter::Linear;
    bound.Minify = sampler.Min == Gltf::Filter::Nearest
        ? Render::SubjectFilter::Nearest
        : Render::SubjectFilter::Linear;
    bound.Mip = sampler.Mip == Gltf::MipFilter::None
        ? Render::SubjectMip::None
        : (sampler.Mip == Gltf::MipFilter::Nearest
               ? Render::SubjectMip::Nearest
               : Render::SubjectMip::Linear);
  }
  return true;
}

}

void ResolveDeclaredSurface(const Render::Shape &geometry, const Material &row,
                            SurfaceTable &out) {
  out.Slots.clear();
  out.Material.clear();
  out.PartSlot.clear();
  out.Decoded.clear();

  out.PartSlot.assign(geometry.Parts.size(), 0u);
  if (geometry.Surfaces.empty()) {
    Render::SubjectMaterial slot;
    slot.Row = row;
    out.Slots.push_back(slot);
    out.Decoded.push_back(SurfaceRasters());
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
      Render::SubjectMaterial surface;
      surface.Row = material >= 0 && (size_t)material < geometry.Surfaces.size()
                        ? geometry.Surfaces[(size_t)material]
                        : row;
      out.Slots.push_back(surface);
      out.Decoded.push_back(SurfaceRasters());
      out.Material.push_back(material);
    }
    out.PartSlot[part] = (uint32_t)slot;
  }
}

void ResolveSurfaceTable(const Gltf::Document &file, const Gltf::Subject &geometry, bool carriesTransmission,
                         bool ownMaterials, SurfaceTable &out) {
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
      surface.Row = Gltf::DefaultMaterial();
      if (material >= 0 && (size_t)material < geometry.Surfaces().size()) {

        surface.Row = geometry.Surfaces()[(size_t)material];
        if (!carriesTransmission) {
          surface.Row.Transmission = 0.0f;
          surface.Row.Thickness = 0.0f;
        }

        if (!ownMaterials) { surface.Row.Alpha = AlphaMode::Opaque; }
      }
      out.Material.push_back(material);
      out.Slots.push_back(surface);
    }
    out.PartSlot[part] = (uint32_t)slot;
  }
}

[[nodiscard]] bool ResolveFileSurface(const Gltf::Document &file, const Gltf::Subject &geometry,
                                      ColourFrom channel, ColourCarrier carrier,
                                      SurfaceTable &table, std::string &error) {
  table.Decoded.assign(table.Slots.size(), SurfaceRasters{});
  const char *socket = channel == ColourFrom::Emissive ? "emissiveTexture" : "baseColorTexture";

  const Gltf::CarriedUvSets carried = geometry.HasUv1()
      ? Gltf::CarriedUvSets::Both
      : Gltf::CarriedUvSets::FirstOnly;
  size_t textured = 0;
  for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
    const int index = table.Material[slot];
    if (index < 0 || (size_t)index >= file.Materials().size()) { continue; }
    const Gltf::MaterialRef &material = file.Materials()[(size_t)index];
    const Gltf::TextureRef &declared =
        channel == ColourFrom::Emissive ? material.Emissive : material.BaseColour;
    if (table.Slots[slot].State().Kind() != SurfaceKind::Opaque &&
        material.BaseColour.Texture != declared.Texture) {
      error = std::string("material '") + material.Name + "' is not OPAQUE, takes its colour from " +
              socket + " " + std::to_string(declared.Texture) + " and its coverage from " +
              "baseColorTexture " + std::to_string(material.BaseColour.Texture) +
              ", and this subject binds one image per surface -- the second binding is the missing "
              "capability, not a texture to substitute";
      return false;
    }
    if (!declared.Declared()) { continue; }
    Render::SubjectTexture &base = table.Slots[slot].Colour;
    std::string why;
    if (!Gltf::UvSetOf(declared, carried, socket, base.Set, why)) {
      error = std::string("material '") + material.Name + "' " + why;
      return false;
    }
    const Gltf::Texture &texture = file.Textures()[(size_t)declared.Texture];
    std::vector<uint8_t> encoded;
    if (!file.ImageBytes(texture.Source, encoded)) {
      error = "material '" + material.Name + "' names image " + std::to_string(texture.Source) +
              ", whose bytes could not be read";
      return false;
    }
    if (!DecodeImage(encoded.data(), encoded.size(), table.Decoded[slot].Colour) ||
        !table.Decoded[slot].Colour.Holds()) {
      error = std::string("the ") + socket + " image of material '" + material.Name + "' is " +
              std::to_string(encoded.size()) + " bytes that this decoder does not read";
      return false;
    }
    base.Rgba = table.Decoded[slot].Colour.Rgba.data();
    base.Width = (uint32_t)table.Decoded[slot].Colour.Width;
    base.Height = (uint32_t)table.Decoded[slot].Colour.Height;

    base.Uv = declared.Transform;
    if (texture.Sampler >= 0) {
      const Gltf::Sampler &sampler = file.Samplers()[(size_t)texture.Sampler];
      base.WrapU = WrapOf(sampler.WrapS);
      base.WrapV = WrapOf(sampler.WrapT);
      base.Magnify = sampler.Mag == Gltf::Filter::Nearest
          ? Render::SubjectFilter::Nearest
          : Render::SubjectFilter::Linear;
      base.Minify = sampler.Min == Gltf::Filter::Nearest
          ? Render::SubjectFilter::Nearest
          : Render::SubjectFilter::Linear;
      base.Mip = sampler.Mip == Gltf::MipFilter::None
          ? Render::SubjectMip::None
          : (sampler.Mip == Gltf::MipFilter::Nearest
                 ? Render::SubjectMip::Nearest
                 : Render::SubjectMip::Linear);
    }
    ++textured;
  }

  if (channel == ColourFrom::Row) {
    for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
      const int index = table.Material[slot];
      if (index < 0 || (size_t)index >= file.Materials().size()) { continue; }
      const Gltf::MaterialRef &material = file.Materials()[(size_t)index];
      table.Slots[slot].NormalScale = (float)material.NormalScale;
      const struct {
        const Gltf::TextureRef &Declared;
        const char *Socket;
        Raster &Into;
        Render::SubjectTexture &Bound;
      } maps[] = {
          {material.Normal, "normalTexture", table.Decoded[slot].Normal, table.Slots[slot].Normal},
          {material.MetallicRoughness, "metallicRoughnessTexture", table.Decoded[slot].MetalRough,
           table.Slots[slot].MetalRough},
          {material.Emissive, "emissiveTexture", table.Decoded[slot].Emissive,
           table.Slots[slot].Emissive},
          {material.SpecularStrength, "specularTexture", table.Decoded[slot].SpecularStrength,
           table.Slots[slot].SpecularStrength},
          {material.SpecularTint, "specularColorTexture", table.Decoded[slot].SpecularTint,
           table.Slots[slot].SpecularTint},
      };
      for (const auto &map : maps) {
        if (!ReadSocketImage(file, material, map.Declared, map.Socket, carried, map.Into, map.Bound,
                             error)) {
          return false;
        }
      }
    }
  }

  if (channel != ColourFrom::Row && carrier == ColourCarrier::Texture && textured == 0) {
    error = std::string("the declaration hands the surface to the file's ") + socket +
            " and no material of it declares one";
    return false;
  }
  if (carrier != ColourCarrier::Texture && textured > 0) {
    error = std::string("the declaration says the appearance is not the ") + socket + " IMAGE and " +
            std::to_string(textured) + " material(s) of the file declare an image on that socket, "
            "which this case would then be sampling instead";
    return false;
  }

  if (carrier == ColourCarrier::VertexColour && !geometry.HasColour()) {
    error = "the declaration says the appearance is carried by COLOR_0 and no primitive of the subject "
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

}
