#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINPATCHERROR_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINPATCHERROR_H

#include "TerrainInstance.h"
#include <array>
#include <cstddef>
#include <span>

namespace outshine::Render {
[[nodiscard]] std::array<float, 4> TerrainPatchHeightErrors(std::span<const float> heights,
                                                            size_t patch);
[[nodiscard]] std::array<float, 4> TerrainPatchWorldErrors(
    const TerrainInstance &tile, size_t patch, const std::array<float, 4> &heightErrors);
}
#endif
