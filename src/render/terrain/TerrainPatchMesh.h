#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINPATCHMESH_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINPATCHMESH_H

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {
using TerrainPatchFace = std::array<uint32_t, 3>;

struct TerrainPatchSample {
  std::array<uint32_t, 6> Nodes;
  std::array<double, 6> Weights;
};

struct TerrainPatchMesh {
#define GROUND_INT(name, value) static constexpr int name = value;
#define GROUND_UINT(name, value) static constexpr uint32_t name = value;
#define GROUND_FLOAT(name, value) static constexpr float name = value;
#include "GroundConstants.inc"
#undef GROUND_INT
#undef GROUND_UINT
#undef GROUND_FLOAT
  static constexpr uint32_t kSide = kGroundPatchCells + 1u;
  static constexpr uint32_t kNodes = kSide * kSide;
  std::vector<TerrainPatchFace> Faces;
  std::vector<TerrainPatchSample> Samples;
};

enum class TerrainPatchMeshError { InvalidStride, Backend, InvalidRefinement };
[[nodiscard]] std::vector<TerrainPatchFace> TerrainPatchFaces(uint32_t stride);
[[nodiscard]] const std::array<uint32_t, 4> &TerrainPatchIndexCounts();
[[nodiscard]] std::expected<TerrainPatchMesh, TerrainPatchMeshError>
MakeTerrainPatchMesh(uint32_t stride);
[[nodiscard]] double
TerrainPatchHeightError(std::span<const float, TerrainPatchMesh::kNodes> heights,
                        std::span<const TerrainPatchSample> samples) noexcept;
}
#endif
