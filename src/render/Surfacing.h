#ifndef OUTSHINE_RENDER_SURFACING_H
#define OUTSHINE_RENDER_SURFACING_H

#include <cstdint>
#include <string>
#include <vector>

#include "Image.h"
#include "Material.h"
#include "stages/SubjectTypes.h"
#include "Shape.h"

namespace outshine::Render {

struct SurfaceRasters {
  Core::Raster Colour;
  Core::Raster Normal;
  Core::Raster MetalRough;
  Core::Raster Emissive;

  Core::Raster SpecularStrength;
  Core::Raster SpecularTint;
};

struct SurfaceTable {
  std::vector<SubjectMaterial> Slots;
  std::vector<int> Material;
  std::vector<uint32_t> PartSlot;
  std::vector<SurfaceRasters> Decoded;
};

enum class ColourFrom { Declared, BaseColour, Emissive, Row };

enum class ColourCarrier { Texture, Factor, VertexColour };

void ResolveDeclaredSurface(const Shape &geometry, const outshine::Material &row, SurfaceTable &out);

}
#endif
