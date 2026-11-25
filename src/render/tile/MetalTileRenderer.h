#ifndef OUTSHINE_RENDER_TILE_METALTILERENDERER_H
#define OUTSHINE_RENDER_TILE_METALTILERENDERER_H

#include "Camera.h"
#include "SurfaceTile.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine::Render {

inline constexpr int TileFrameWidth = 1280;
inline constexpr int TileFrameHeight = 720;

struct MetalTileFrames {
  std::string Device;
  double SetupMs = 0;
  double UploadMs = 0;
  size_t UploadedBytes = 0;
  std::vector<double> GpuMs;
  std::vector<uint8_t> Rgba;
};

[[nodiscard]] std::expected<MetalTileFrames, std::string>
RenderMetalTile(const SurfaceTile &tile, std::span<const Camera> cameras, bool readImage);

}
#endif
