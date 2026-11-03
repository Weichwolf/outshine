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

static_assert(sizeof(ClipmapVertex) == 2 * sizeof(float));
static_assert(alignof(ClipmapVertex) == alignof(float));
static_assert(offsetof(ClipmapVertex, East) == 0);
static_assert(offsetof(ClipmapVertex, North) == sizeof(float));

struct ClipmapShape {
  uint32_t HalfCells = 0;
  std::optional<std::array<uint32_t, 2>> HolePhase = std::nullopt;
};

enum class ClipmapTopologyError { InvalidShape, OverBudget };

struct ClipmapTopology {
  std::vector<ClipmapVertex> Vertices;
  std::vector<uint32_t> Indices;

  [[nodiscard]] static std::expected<ClipmapTopology, ClipmapTopologyError> Make(ClipmapShape shape,
                                                                                 size_t bytesMost);
};
}
#endif
