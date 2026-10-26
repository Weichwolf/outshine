#ifndef OUTSHINE_EXPERIMENT_SOURCE_RASTER_INPUT_H
#define OUTSHINE_EXPERIMENT_SOURCE_RASTER_INPUT_H
#include <array>
#include <cstdint>
#include <vector>

namespace SourceRaster {
constexpr uint32_t TileSide = 8;
constexpr uint32_t Tiles = 64;
constexpr uint32_t Side = Tiles * TileSide;
constexpr uint32_t CaseCount = Tiles * Tiles;

struct alignas(16) Quad {
  std::array<std::array<float, 4>, 4> Points;
};

static_assert(sizeof(Quad) == 64 && alignof(Quad) == 16);
[[nodiscard]] std::vector<Quad> Generate(uint32_t seed, bool normalize);
}
#endif
