#ifndef OUTSHINE_RENDER_STAGES_GROUNDPATCHMESH_H
#define OUTSHINE_RENDER_STAGES_GROUNDPATCHMESH_H

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {
using GroundPatchFace = std::array<uint32_t, 3>;

struct GroundPatchSample {
  std::array<uint32_t, 6> Nodes;
  std::array<double, 6> Weights;
};

struct GroundPatchMesh {
#define GROUND_INT(name, value) static constexpr int name = value;
#define GROUND_UINT(name, value) static constexpr uint32_t name = value;
#define GROUND_FLOAT(name, value) static constexpr float name = value;
#include "GroundConstants.inc"
#undef GROUND_INT
#undef GROUND_UINT
#undef GROUND_FLOAT
  static constexpr uint32_t kSide = kGroundPatchCells + 1u;
  static constexpr uint32_t kNodes = kSide * kSide;
  std::vector<GroundPatchFace> Faces;
  std::vector<GroundPatchSample> Samples;
};

enum class GroundPatchMeshError { InvalidStride, Backend, InvalidRefinement };
[[nodiscard]] std::vector<GroundPatchFace> GroundPatchFaces(uint32_t stride);
[[nodiscard]] const std::array<uint32_t, 4> &GroundPatchIndexCounts();
[[nodiscard]] std::expected<GroundPatchMesh, GroundPatchMeshError>
MakeGroundPatchMesh(uint32_t stride);
[[nodiscard]] double GroundPatchHeightError(std::span<const float, GroundPatchMesh::kNodes> heights,
                                            std::span<const GroundPatchSample> samples) noexcept;
}
#endif
