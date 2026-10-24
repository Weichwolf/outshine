#ifndef OUTSHINE_RENDER_TERRAIN_TERRAININSTANCE_H
#define OUTSHINE_RENDER_TERRAIN_TERRAININSTANCE_H

#include "Mat4.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace outshine::Render {
struct TerrainInstance {
  Mat4f Row = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  std::array<float, 8> Corners = {{}};
  float Page = 0.0f;
  float SagInv = 0.0f;
  float StepE = 0.0f;
  float StepN = 0.0f;
};

inline constexpr uint32_t kGroundInstanceFloats = 28;
inline constexpr size_t kGroundPageOffset = 96;
inline constexpr size_t kGroundStepNOffset = 108;
static_assert(sizeof(TerrainInstance) == kGroundInstanceFloats * sizeof(float),
              "an instance is the twenty-eight floats the lattice's vertex shader reads");
static_assert(offsetof(TerrainInstance, Corners) == 64);
static_assert(offsetof(TerrainInstance, Page) == kGroundPageOffset);
static_assert(offsetof(TerrainInstance, StepN) == kGroundStepNOffset);

}
#endif
