/* THE SURFACES A SUBJECT DRAWS WITH, RESOLVED FROM ITS OWN DOCUMENT (board:1455).
 *
 * **THIS IS THE CONTENT SURFACE MEETING THE DEVICE SURFACE, so it is the engine's.** glTF is one of
 * the three interfaces this engine is built on and `Render::SubjectMaterial` is the other end of the
 * translation: a row of numbers and some images, with no field that can switch pipeline state. Every
 * client that ever loads a document needs exactly this walk -- materials to slots, sockets to UV
 * sets, samplers to filters -- so a consumer doing it itself would be re-implementing the reader.
 *
 * IT MOVED HERE OUT OF THE RENDER HARNESS, which is the one place it had ever been written, and the
 * harness now calls it like anybody else. **What turns a document into surfaces is the engine's;
 * what SCORES the picture is the harness's**, and that is the whole of the line.
 *
 * It reads a document and produces a renderer's material, so it sits in the clients layer beside the
 * declared compositor that hands those over -- not in `src/gltf`, which knows no subject material. */
#ifndef SURFACES_H
#define SURFACES_H

#include <cstdint>
#include <string>
#include <vector>

#include "Document.h"
#include "Image.h"
#include "Renderer.h"
#include "Subject.h"

namespace outshine::Clients {

/* THE SURFACES ONE SUBJECT DRAWS WITH: one slot per material any drawn primitive names, and
 * `PartSlot` is which slot each part draws with. Two primitives of one material share a slot, which
 * is what lets the compiled draw list merge them into one call. The decoded rasters are held here
 * because the renderer copies them and the studio only points at them. */
/* THE THREE DECODED IMAGES ONE SURFACE MAY WEAR, held together because they belong to one surface:
 * three vectors indexed by slot would be three things to keep in step, and the slot is the thing. */
struct SurfaceRasters {
  Raster Colour;
  Raster Normal;
  Raster MetalRough;
  Raster Emissive;
  /* `KHR_materials_specular`'s two (board:1205), decoded per slot like the rest. */
  Raster SpecularStrength;
  Raster SpecularTint;
};

struct SurfaceTable {
  std::vector<Render::SubjectMaterial> Slots;
  std::vector<int> Material;      /* the document's material index per slot, -1 where none */
  std::vector<uint32_t> PartSlot;
  std::vector<SurfaceRasters> Decoded;
};

/* WHICH OF THE FILE'S OWN CHANNELS THE DECLARED RADIANCE IS TAKEN FROM, and it is a three-valued
 * question that no boolean can carry (`Enum.2`). `Declared` is the arm where the manifest states the
 * colours itself and the file's materials are not read for appearance at all. The other two name a
 * glTF socket, and which one is a property of the ASSET: `TextureLinearInterpolationTest` states its
 * whole picture in `emissiveFactor`/`emissiveTexture` over a base colour of `[0,0,0,1]`, so a runner
 * that could only read base colour would render its two spheres black and score that. */
enum class ColourFrom { Declared, BaseColour, Emissive, Row };

/* WHERE THE NAMED SOCKET'S VALUE COMES FROM IN THAT FILE, and the case says which because the three
 * are different subjects. `Texture` is the arm the socket arms were built for -- the picture IS the
 * image, and a case that declared it and found no image would be scoring a flat factor while
 * claiming to score a texture. `Factor` is the arm `EmissiveStrengthTest` needs: five cubes whose
 * whole appearance is `emissiveFactor` times `KHR_materials_emissive_strength`, with no image
 * anywhere in the file. `VertexColour` is the arm `BoxVertexColors` needs (board:1193): the picture
 * is the primitive's own `COLOR_0` multiplied into base colour, so a case declaring `factor` there
 * would be naming the wrong operand in the one field that says where the appearance comes from.
 * None is a default, so the mismatch in any direction is a refusal naming the file rather than a
 * picture nobody looks at. */
enum class ColourCarrier { Texture, Factor, VertexColour };

/* THE SLOTS EVERY DRAWN PRIMITIVE NAMES, and which slot each part draws with. `carriesTransmission`
 * false zeroes the transmission and thickness of every row, and `ownMaterials` false makes every row
 * opaque -- a consumer that replaces the appearance is not honouring the file's coverage either. */
void ResolveSurfaceTable(const Gltf::Document &file, const Gltf::Subject &geometry,
                         bool carriesTransmission, bool ownMaterials, SurfaceTable &out);

/* THE IMAGES THOSE SLOTS SAMPLE, decoded and bound. `ColourFrom::Row` is the ordinary path -- the
 * material's whole row, with the normal, metallic-roughness, emissive and two specular maps beside
 * it; the socket arms are for a consumer that states where the appearance lives. Refuses a socket
 * the subject carries no UV set for, an image the decoder does not read, and a declaration that
 * disagrees with the file in either direction. */
[[nodiscard]] bool ResolveFileSurface(const Gltf::Document &file, const Gltf::Subject &geometry,
                                      ColourFrom channel, ColourCarrier carrier, SurfaceTable &table,
                                      std::string &error);

} // namespace outshine::Clients
#endif
