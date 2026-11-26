#ifndef OUTSHINE_RENDER_PRODUCTS_METALPRODUCTRENDERER_H
#define OUTSHINE_RENDER_PRODUCTS_METALPRODUCTRENDERER_H

#include "Mat4.h"
#include "FarPanorama.h"
#include "GroupImpostorView.h"
#include "ImpostorViewDemand.h"
#include "SurfaceTile.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
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
  size_t TileTriangles = 0;
  size_t GroupTriangles = 0;
  size_t BlendingGroups = 0;
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

using MetalSurfaceInstance = SurfaceInstance;

struct MetalSurfaceDetail {
  Vec3 WorldCentreM;
  ImpostorDistanceRamp Ramp;
  bool Coarse;
};

struct MetalSurface {
  const SurfaceTile &Product;
  Mat4 WorldFromTile;
  std::optional<size_t> HandoffGroup;
  std::span<const MetalSurfaceInstance> Instances;
  std::optional<MetalSurfaceDetail> Detail = std::nullopt;
};

struct MetalPlacedSurface {
  std::shared_ptr<const SurfaceTile> Product;
  Mat4 WorldFromTile;
  std::optional<size_t> HandoffGroup;
  std::vector<MetalSurfaceInstance> Instances;
  std::optional<MetalSurfaceDetail> Detail = std::nullopt;
};

struct MetalPlacedImpostorGroup {
  std::vector<GroupImpostorView> Views;
  Mat4 WorldFromTile;
  Vec3 CentreM;
  double HalfExtentM;
  std::optional<ImpostorDistanceRamp> Handoff;
  bool FractionalCoverage = false;
};

struct MetalPlacedCut {
  std::vector<MetalPlacedSurface> Tiles;
  std::vector<MetalPlacedImpostorGroup> Groups;
};

using MetalSurfaceLoad = std::function<std::expected<MetalPlacedCut, std::string>()>;

struct MetalSurfaceStream {
  MetalSurfaceLoad Load;
  double FrameSeconds = 0;
};

struct MetalImpostorGroup {
  std::span<const GroupImpostorView> Views;
  Mat4 WorldFromTile;
  Vec3 CentreM;
  double HalfExtentM;
  std::optional<ImpostorDistanceRamp> Handoff;
  bool FractionalCoverage = false;
};

struct MetalProductInputs {
  std::span<const MetalSurface> Tiles;
  std::span<const FarPanorama> Panoramas;
  std::span<const MetalImpostorGroup> Groups;
};

struct MetalProductView {
  Mat4 ClipFromWorld;
  Vec3 EyeM;
  PanoramaView Panorama;
  size_t PanoramaIndex = 0;
};

using MetalProductViewAt = std::function<std::expected<MetalProductView, std::string>(size_t)>;
using MetalSurfaceFaceWriter =
    std::function<std::expected<void, std::string>(size_t, std::span<const float>)>;

[[nodiscard]] std::expected<MetalProductFrames, std::string>
RenderMetalProducts(const MetalProductInputs &products,
                    size_t count,
                    const MetalProductViewAt &viewAt,
                    bool readImage,
                    const MetalSurfaceStream &stream = {});

[[nodiscard]] std::expected<MetalProductFrames, std::string>
CaptureMetalSurfaceFaces(const MetalProductInputs &products,
                         const PanoramaLighting &lighting,
                         size_t side,
                         std::span<const MetalProductView> views,
                         const MetalSurfaceFaceWriter &writeFace);

}
#endif
