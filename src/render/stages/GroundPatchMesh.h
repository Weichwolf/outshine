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
  std::vector<GroundPatchFace> Faces;
  std::vector<GroundPatchSample> Samples;
};

enum class GroundPatchMeshError { InvalidStride, Backend, InvalidRefinement };
[[nodiscard]] std::vector<GroundPatchFace> GroundPatchFaces(uint32_t stride);
[[nodiscard]] std::expected<GroundPatchMesh, GroundPatchMeshError>
MakeGroundPatchMesh(uint32_t stride);
[[nodiscard]] double GroundPatchHeightError(std::span<const float, 81> heights,
                                            std::span<const GroundPatchSample> samples) noexcept;
}
#endif
