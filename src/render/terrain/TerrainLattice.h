#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINLATTICE_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINLATTICE_H

#include <cstdint>

namespace outshine::Render::TerrainLattice {
#define GROUND_INT(name, value) static constexpr int name = value;
#define GROUND_UINT(name, value) static constexpr uint32_t name = value;
#define GROUND_FLOAT(name, value) static constexpr float name = value;
#include "GroundConstants.inc"
#undef GROUND_INT
#undef GROUND_UINT
#undef GROUND_FLOAT
inline constexpr int kSide = kGroundSide;
inline constexpr int kPageSide = kSide + 2;
inline constexpr uint32_t kPageNodes =
    static_cast<uint32_t>(kPageSide) * static_cast<uint32_t>(kPageSide);
inline constexpr float kSkirtSteps = kGroundSkirtSteps;
inline constexpr uint32_t kNodes = static_cast<uint32_t>(kSide) * static_cast<uint32_t>(kSide);
inline constexpr uint32_t kPageColumns = kGroundPageColumns;
inline constexpr uint32_t kPagesPerLayer = kPageColumns * kPageColumns;
inline constexpr uint32_t kMaximumPageLayers = 256;
inline constexpr uint32_t kMaximumPages = kMaximumPageLayers * kPagesPerLayer;
inline constexpr uint32_t kGridFloats = 3;
inline constexpr uint32_t kVertices = kNodes + 4u * static_cast<uint32_t>(kSide);
inline constexpr uint32_t kQuads =
    static_cast<uint32_t>(kSide - 1) * static_cast<uint32_t>(kSide - 1);
inline constexpr uint32_t kSkirtQuads = 4u * static_cast<uint32_t>(kSide - 1);
inline constexpr uint32_t kIndices = (kQuads + kSkirtQuads) * 6u;
inline constexpr uint32_t kPatchSide = kGroundPatchCells + 1u;
inline constexpr uint32_t kPatchCount = 1u << kGroundPatchBits;
inline constexpr uint32_t kPatchIndices = kGroundPatchCells * kGroundPatchCells * 6u;
}
#endif
