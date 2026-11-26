#ifndef OUTSHINE_RENDER_PRODUCTS_METALPRODUCTRENDERER_H
#define OUTSHINE_RENDER_PRODUCTS_METALPRODUCTRENDERER_H

#include "Mat4.h"
#include "FarPanorama.h"
#include "SurfaceTile.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Render {

inline constexpr int MetalProductFrameWidth = 1280;
inline constexpr int MetalProductFrameHeight = 720;
inline constexpr size_t MetalProductFramesInFlight = 3;

struct MetalProductFrameSample {
  double WallMs = 0;
  double RenderMs = 0;
  double SyncWaitMs = 0;
  std::optional<double> GpuMs;
};

struct MetalProductFrames {
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
  std::vector<MetalProductFrameSample> Samples;
  std::vector<uint8_t> Rgba;
};

struct PanoramaView {
  Vec3 Forward;
  Vec3 RightSpan;
  Vec3 UpSpan;
  Vec3 EyeOffsetM;
  double FocalPixels;
  double DepthScaleM;
  double DepthBias;
  double Exposure;
};

using PanoramaViewAt = std::function<std::expected<PanoramaView, std::string>(size_t)>;

[[nodiscard]] std::expected<MetalProductFrames, std::string> RenderMetalPanorama(
    const FarPanorama &panorama, size_t count, const PanoramaViewAt &viewAt, bool readImage);

using TileClipAt = std::function<std::expected<Mat4, std::string>(size_t)>;

[[nodiscard]] std::expected<MetalProductFrames, std::string>
RenderMetalTile(const SurfaceTile &tile, size_t count, const TileClipAt &clipAt, bool readImage);

}
#endif
