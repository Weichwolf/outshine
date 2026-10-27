#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALRESOLUTION_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALRESOLUTION_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Material.h"
#include "Geometry.h"
#include "material/MaterialBinding.h"
#include "RenderGeometryView.h"

namespace outshine::Render {

struct SurfaceTable {
  std::vector<MaterialBinding> Slots;
  std::vector<int> Material;
  std::vector<int> NativeMaterial;
  std::vector<uint32_t> PartSlot;
};

[[nodiscard]] bool ResolveNativeTextures(std::span<const ImageView> images,
                                         std::span<MaterialBinding> surfaces,
                                         std::string &error);

[[nodiscard]] bool ResolveNativeTextures(const Geometry &geometry,
                                         std::span<MaterialBinding> surfaces,
                                         std::string &error);

void ResolveDeclaredSurface(const RenderGeometryView &geometry,
                            const outshine::Material &row,
                            SurfaceTable &out);

}
#endif
