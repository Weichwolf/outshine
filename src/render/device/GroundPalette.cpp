#include "GroundPalette.h"
#include "VegetationTemplates.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace outshine::Render {
namespace {
struct LitterLayer {
  std::array<float, 4> Difference;
  std::array<float, 4> Surface;
};

constexpr size_t kPaletteStride = 4u;
constexpr size_t kSurfaceStride = 8u;
constexpr size_t kLayerStride = 8u;
constexpr float kVerticalSlopeDeg = 90.0f;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 &&
              sizeof(LitterLayer) == kLayerStride * sizeof(float) && alignof(LitterLayer) == 4 &&
              offsetof(LitterLayer, Difference) == 0 && offsetof(LitterLayer, Surface) == 16);
}

std::vector<float> GroundMaterialPalette(const Ground::VegetationTemplates &wearing,
                                         const Vec4f &fallback) {
  const size_t rows = wearing.TemplateCount();
  const size_t slopesAt = kPaletteStride * (rows + 2u);
  const size_t surfacesAt = slopesAt + rows + 1u;
  const size_t layersAt = surfacesAt + kSurfaceStride * (rows + 1u);
  std::vector<float> palette(layersAt + kLayerStride * (rows + 1u), 0.0f);
  palette[0] = std::bit_cast<float>(static_cast<uint32_t>(rows));
  palette[1] = std::bit_cast<float>(static_cast<uint32_t>(wearing.RockTemplate()));
  palette[2] = wearing.Limit().SlopeBandDeg();
  palette[3] = std::bit_cast<float>(static_cast<uint32_t>(layersAt));
  for (size_t row = 0; row < rows; ++row) {
    const auto &source = wearing.Rows()[row];
    const auto appearance = source.MeanGround();
    LitterLayer layer;
    for (size_t channel = 0; channel < kPaletteStride; ++channel) {
      palette[kPaletteStride * (row + 1u) + channel] = appearance[channel];
      palette[surfacesAt + kSurfaceStride * row + channel] = source.GroundSurf[channel];
      layer.Difference[channel] = source.Litter[channel] - source.Ground[channel];
      layer.Surface[channel] = source.LitterSurf[channel];
    }
    std::memcpy(palette.data() + layersAt + kLayerStride * row, &layer, sizeof(layer));
    palette[slopesAt + row] = source.Edge[3];
    palette[surfacesAt + kSurfaceStride * row + 4u] = source.Mix[1];
    palette[surfacesAt + kSurfaceStride * row + 5u] = source.Edge[0];
    palette[surfacesAt + kSurfaceStride * row + 6u] = source.Edge[1];
    palette[surfacesAt + kSurfaceStride * row + 7u] = source.Mix[0];
  }
  for (size_t channel = 0; channel < kPaletteStride; ++channel) {
    palette[kPaletteStride * (rows + 1u) + channel] = fallback[channel];
  }
  palette[slopesAt + rows] = kVerticalSlopeDeg;
  return palette;
}
}
