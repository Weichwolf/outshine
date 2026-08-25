#ifndef OUTSHINE_ENGINE_SURFACES_H
#define OUTSHINE_ENGINE_SURFACES_H

#include <cstdint>
#include <string>
#include <vector>

#include "Document.h"
#include "Material.h"
#include "Image.h"
#include "Renderer.h"
#include "Subject.h"

namespace outshine::Clients {

struct SurfaceRasters {
  Raster Colour;
  Raster Normal;
  Raster MetalRough;
  Raster Emissive;

  Raster SpecularStrength;
  Raster SpecularTint;
};

struct SurfaceTable {
  std::vector<Render::SubjectMaterial> Slots;
  std::vector<int> Material;
  std::vector<uint32_t> PartSlot;
  std::vector<SurfaceRasters> Decoded;
};

enum class ColourFrom { Declared, BaseColour, Emissive, Row };

enum class ColourCarrier { Texture, Factor, VertexColour };

void ResolveSurfaceTable(const Gltf::Document &file, const Gltf::Subject &geometry,
                         bool carriesTransmission, bool ownMaterials, SurfaceTable &out);

void ResolveDeclaredSurface(const Gltf::Subject &geometry, const Material &row, SurfaceTable &out);

[[nodiscard]] bool ResolveFileSurface(const Gltf::Document &file, const Gltf::Subject &geometry,
                                      ColourFrom channel, ColourCarrier carrier, SurfaceTable &table,
                                      std::string &error);

}
#endif
