#include "Surfaces.h"

namespace outshine::Clients {
namespace {

Render::SubjectWrap WrapOf(Gltf::Wrap wrap) {
  switch (wrap) {
    case Gltf::Wrap::ClampToEdge: return Render::SubjectWrap::ClampToEdge;
    case Gltf::Wrap::MirroredRepeat: return Render::SubjectWrap::MirroredRepeat;
    case Gltf::Wrap::Repeat: return Render::SubjectWrap::Repeat;
  }
  return Render::SubjectWrap::Repeat;
}

/* ONE OF THE FILE'S OWN MAPS INTO ONE SURFACE SLOT, WHICHEVER SOCKET IT SITS IN. It is a different
 * function from the colour one because it is a different question: there is no alpha and no coverage
 * to decide, and a socket that declares nothing is an ordinary material rather than a refusal --
 * glTF's defaults are the factors, and white is what the shader multiplies by when no image is bound.
 *
 * THE sRGB TRANSFER IS NOT DECIDED HERE AND CANNOT BE. What crosses is the file's RGBA8 texels; which
 * of them carry the transfer is a property of the socket, and `SubjectDraw::Upload` is where the
 * socket is named. A `Linear` in this function's name was true of the two maps it had and would be a
 * lie about the third (`NL.1`). */
[[nodiscard]] bool ReadSocketImage(const Gltf::Document &file, const Gltf::MaterialRef &material,
                                   const Gltf::TextureRef &declared, const char *socket,
                                   Gltf::CarriedUvSets carried,
                                   Raster &raster,
                                   Render::SubjectTexture &bound, std::string &error) {
  if (!declared.Declared()) { return true; }
  /* WHICH UV SET, ANSWERED BY THE LIBRARY AND NOT HERE (board:1182). The runner is a consumer of the
   * reader's answer; a second mapping written in this file is a second place the narrowing could
   * drift, and it is the narrowing rather than the refusal that this task changed. */
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
  /* PER REFERENCE, NOT PER MATERIAL (board:1177): the transform crosses beside the image it belongs
   * to, so a socket's matrix reaches its own sampler and no other. */
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

} // namespace

/* THE SURFACE TABLE THE SUBJECT DRAWS WITH: one slot per material any drawn primitive names, in the
 * order the parts first name them. Two primitives of one material get one slot, which is what lets
 * the compiled draw list merge them into one call, and a primitive that names no material gets a
 * slot of glTF's OWN default material -- which is a surface, not an absence.
 *
 * THE FORMAT'S DEFAULT AND NOT THE ENGINE'S (board:1193, `Gltf::DefaultMaterial`). This runner used
 * `outshine::Material{}` here, a mid-grey dielectric, which is what this engine draws for a surface
 * nobody described; the FORMAT says a primitive with no material wears `baseColorFactor [1,1,1,1]`.
 * `BoxVertexColors` declares no material at all, so the difference was a factor of two on every
 * channel of its whole body -- and it would have read as our COLOR_0 being wrong. */
/* `carriesTransmission` IS WHETHER THE CASE'S OWN RECIPE ALLOWED A TRANSMISSION BOUNCE (board:1386). At
 * zero -- which is the corpus's default -- the oracle CANNOT show anything through a surface, so its
 * glass is an opaque body whatever the file says. Handing this engine the FILE's transmissive row
 * there asks it to see through a surface the other side does not, and the renderer is right to refuse
 * a refracting volume no pass draws.
 *
 * IT IS THE RECIPE AND NOT WHERE THE COLOUR CAME FROM, and that distinction was measured: the first
 * condition asked whether the materials were the file's, and `ABeautifulGame` takes its colour from
 * the file's own base-colour images through an EMITTER at zero bounces -- so it passed that test,
 * drew its glass, and entered the red set at 19.542392 px where it had never been. **The question was
 * always what the oracle was allowed to do.**
 *
 * SO THE ARM OWNS THE CLOSURE AND NOT ONLY THE COLOUR. A coverage case already overrides what a
 * surface emits; what it did not override is what KIND of surface it is, and those are one statement.
 * The transmissive fields are cleared and nothing else is touched: the geometry, the alpha mode and
 * every texture reference stay the file's, because those are what a coverage case IS about. */
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
      if (material >= 0 && (size_t)material < file.Materials().size()) {
        /* THE WHOLE ROW AND NOT A CHANNEL OF IT. The coverage factor is then `baseColorFactor.a` by
         * construction whatever the colour channel is, which is glTF's own rule: alpha comes from
         * the base colour and from nowhere else, even where the picture is stated in emissive. */
        surface.Row = file.Materials()[(size_t)material].Surface;
        if (!carriesTransmission) {
          surface.Row.Transmission = 0.0f;
          surface.Row.Thickness = 0.0f;
        }
        /* AND THE ALPHA MODE IS THE ARM'S FOR THE SAME REASON (board:1425). A case whose materials are
         * the MANIFEST's replaces every surface with a flat emitter, and the preparer's emission arm
         * carries no coverage at all -- so the reference is opaque whatever the file says, and honouring
         * `alphaMode: BLEND` here makes this engine see through a surface the other side does not.
         *
         * [MEASURED] `GlassVaseFlowers` is the asset that shows it: its two vases are the two ways to
         * make glass -- `GlassAlpha` with `alphaMode BLEND` and a base alpha of **0.3**, and
         * `GlassTransmission` with the extension. Ours drew the stems THROUGH the first and the
         * reference did not, and the two pictures say so at a glance. */
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
  /* WHAT THE SUBJECT CARRIES, ASKED ONCE (board:1182): it is the same answer for every socket of
   * every material of this subject, and asking it per reference would be one place per socket for it
   * to be asked differently. */
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
    /* board:1177 -- the socket the case declared its picture in carries its own transform, so a
     * `gltf-base-colour` case reads the file's `KHR_texture_transform` on the socket it names. */
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

  /* THE OTHER THREE MAPS, AND ONLY UNDER THE ARM THAT SHADES WITH THE FILE'S OWN ROW. The other two
   * arms REPLACE the closure -- a diffuse or an emissive one -- so a normal map they decoded would
   * be an image nothing reads, and `SciFiHelmet` says so in its own manifest rather than binding
   * one silently.
   *
   * THE EMISSIVE IMAGE IS HERE AND NOT WITH THE COLOUR, because under THIS arm it is not the colour:
   * `emissiveFactor * emissiveTexture` is a radiance added to what the BRDF returns, and the socket
   * arms above take the emissive INSTEAD of the closure. `BoomBox`, `Lantern` and `WaterBottle` all
   * state `emissiveFactor` as `[1, 1, 1]` and put the whole picture of the glow in the image, so a
   * row read without it emits white over the entire body. */
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

  /* A TEXTURE IS OWED WHERE THE CASE SAYS THE PICTURE IS ONE, in both directions. Under `gltf` the
   * row is the appearance and a material with no image is an ordinary material, so nothing is owed;
   * under the two socket arms the case has declared which of the two it reads, and a file that
   * disagrees with its own case's declaration is what stops here. */
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
  /* THE VERTEX-COLOUR ARM OWES THE ATTRIBUTE IT IS NAMED AFTER (board:1193). A case that declared it
   * over a subject carrying no `COLOR_0` would render the factor alone and score a flat colour under
   * the name of a multiplier -- which is the same silent success the two arms above are guarded
   * against in both directions. */
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

} // namespace outshine::Clients
