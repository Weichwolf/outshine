#ifndef OUTSHINE_RENDER_DEVICE_GROUNDPALETTE_H
#define OUTSHINE_RENDER_DEVICE_GROUNDPALETTE_H

#include "math/Vec4.h"
#include <vector>

namespace outshine::Ground {
class VegetationTemplates;
}

namespace outshine::Render {
[[nodiscard]] std::vector<float> GroundMaterialPalette(const Ground::VegetationTemplates &wearing,
                                                       const Vec4f &fallback);
}
#endif
