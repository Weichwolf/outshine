#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPTOPOLOGY_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPTOPOLOGY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace outshine::Render {
struct ClipmapVertex {
  float East = 0;
  float North = 0;
};

static_assert(sizeof(ClipmapVertex) == 8);
static_assert(alignof(ClipmapVertex) == 4);
static_assert(offsetof(ClipmapVertex, East) == 0);
static_assert(offsetof(ClipmapVertex, North) == 4);

struct ClipmapShape {
  uint32_t HalfCells = 0;
  std::optional<std::array<uint32_t, 2>> HolePhase = std::nullopt;
};

enum class ClipmapTopologyError { InvalidShape, OverBudget };

struct ClipmapTopologySize {
  size_t VertexBytes = 0;
  size_t IndexBytes = 0;
  size_t BlockBytes = 0;
  size_t ScratchBytes = 0;
};

struct ClipmapBlock {
  uint32_t First = 0;
  uint32_t Count = 0;
  std::array<float, 2> Low{}, High{};
};

struct ClipmapTopology {
  std::vector<ClipmapVertex> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<ClipmapBlock> Blocks;

  [[nodiscard]] static std::expected<ClipmapTopologySize, ClipmapTopologyError>
  Measure(ClipmapShape shape);

  [[nodiscard]] static std::expected<ClipmapTopology, ClipmapTopologyError> Make(ClipmapShape shape,
                                                                                 size_t bytesMost);
};
}
#endif
