#ifndef OUTSHINE_BASE_MATERIALMAPS_H
#define OUTSHINE_BASE_MATERIALMAPS_H

#include "scene/Material.h"
#include <array>

namespace outshine {
[[nodiscard]] inline auto MaterialMaps(auto &row) noexcept {
  return std::array{&row.BaseColourMap,
                    &row.NormalMap,
                    &row.MetalRoughMap,
                    &row.EmissiveMap,
                    &row.OcclusionMap,
                    &row.SpecularStrengthMap,
                    &row.SpecularTintMap,
                    &row.DiffuseReflectionMap,
                    &row.DiffuseTransmissionMap};
}
}
#endif
