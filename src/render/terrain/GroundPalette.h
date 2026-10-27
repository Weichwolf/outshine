#ifndef OUTSHINE_RENDER_TERRAIN_GROUNDPALETTE_H
#define OUTSHINE_RENDER_TERRAIN_GROUNDPALETTE_H

#include "Vec4.h"
#include "SurfaceMaterial.h"
#include <vector>

namespace outshine::Render {
[[nodiscard]] std::vector<float> GroundMaterialPalette(Ground::SurfacePaletteView surfaces,
                                                       const Vec4f &fallback);
}
#endif
