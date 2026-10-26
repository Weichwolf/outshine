#ifndef OUTSHINE_RENDER_SURFACING_H
#define OUTSHINE_RENDER_SURFACING_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Material.h"
#include "Geometry.h"
#include "stages/SubjectTypes.h"
#include "Shape.h"

namespace outshine::Render {

struct SurfaceTable {
  std::vector<SubjectMaterial> Slots;
  std::vector<int> Material;
  std::vector<int> NativeMaterial;
  std::vector<uint32_t> PartSlot;
};

[[nodiscard]] bool ResolveNativeTextures(std::span<const ImageView> images,
                                         std::span<SubjectMaterial> surfaces,
                                         std::string &error);

[[nodiscard]] bool ResolveNativeTextures(const Geometry &geometry,
                                         std::span<SubjectMaterial> surfaces,
                                         std::string &error);

void ResolveDeclaredSurface(const Shape &geometry,
                            const outshine::Material &row,
                            SurfaceTable &out);

}
#endif
