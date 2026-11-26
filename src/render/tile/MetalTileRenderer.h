#ifndef OUTSHINE_RENDER_TILE_METALTILERENDERER_H
#define OUTSHINE_RENDER_TILE_METALTILERENDERER_H

#include "Mat4.h"
#include "SurfaceTile.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Render {

inline constexpr int TileFrameWidth = 1280;
inline constexpr int TileFrameHeight = 720;
inline constexpr size_t TileFramesInFlight = 3;

struct MetalTileFrameSample {
  double WallMs = 0;
  double RenderMs = 0;
  double SyncWaitMs = 0;
  std::optional<double> GpuMs;
};

struct MetalTileFrames {
  std::string Device;
  uint64_t DeviceId = 0;
  uint64_t ResourceBytes = 0;
  uint64_t RecommendedResourceBytes = 0;
  bool UnifiedMemory = false;
  double SetupMs = 0;
  double UploadMs = 0;
  size_t UploadedBytes = 0;
  double WindowMs = 0;
  double FlushMs = 0;
  std::vector<MetalTileFrameSample> Samples;
  std::vector<uint8_t> Rgba;
};

using TileClipAt = std::function<std::expected<Mat4, std::string>(size_t)>;

[[nodiscard]] std::expected<MetalTileFrames, std::string>
RenderMetalTile(const SurfaceTile &tile, size_t count, const TileClipAt &clipAt, bool readImage);

}
#endif
