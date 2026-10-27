#ifndef OUTSHINE_RENDER_TERRAIN_GROUNDTILE_H
#define OUTSHINE_RENDER_TERRAIN_GROUNDTILE_H

#include <cstdint>
#include "TerrainInstance.h"

namespace outshine::Render {

struct GroundTile {
  TerrainInstance Instance;
  float LowM = 0.0f;
  float HighM = 0.0f;
};

using PageId = uint32_t;
inline constexpr PageId kNoPage = ~0u;
}

#endif
