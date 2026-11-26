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
#include <span>
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
  bool Refined = true;
};

struct MetalSurfaceTransition {
  std::optional<size_t> FirstFrame;
  size_t Tiles = 0;
  size_t ReusedTiles = 0;
  size_t Triangles = 0;
  double ReadyMs = 0;
  double UploadMs = 0;
  std::string Error;
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
  std::optional<MetalSurfaceTransition> Transition;
  std::vector<MetalProductFrameSample> Samples;
  std::vector<uint8_t> Rgba;
};

struct PanoramaView {
  Vec3 Forward;
  Vec3 RightSpan;
  Vec3 UpSpan;
  Vec3 EyeOffsetM;
  Vec3 NextEyeOffsetM{};
  double FocalPixels;
  double DepthScaleM;
  double DepthBias;
  double Exposure;
  double NextWeight = 0;
  double AxialNearM = 0;
  double AxialOverlapEndM = 0;
  double CoverageFadeM = 0;
};

struct MetalSurface {
  const SurfaceTile &Product;
  Mat4 WorldFromTile;
};

struct MetalPlacedSurface {
  SurfaceTile Product;
  Mat4 WorldFromTile;
};

using MetalSurfaceLoad =
    std::function<std::expected<std::vector<MetalPlacedSurface>, std::string>()>;

struct MetalSurfaceStream {
  MetalSurfaceLoad Load;
  double FrameSeconds = 0;
};

struct MetalProductInputs {
  std::span<const MetalSurface> Tiles;
  std::span<const FarPanorama> Panoramas;
};

struct MetalProductView {
  Mat4 ClipFromWorld;
  Vec3 EyeM;
  PanoramaView Panorama;
  size_t PanoramaIndex = 0;
};

using MetalProductViewAt = std::function<std::expected<MetalProductView, std::string>(size_t)>;

[[nodiscard]] std::expected<MetalProductFrames, std::string>
RenderMetalProducts(const MetalProductInputs &products,
                    size_t count,
                    const MetalProductViewAt &viewAt,
                    bool readImage,
                    const MetalSurfaceStream &stream = {});

}
#endif
