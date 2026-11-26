#include "MetalTileRenderer.h"
#include "Units.h"

#import <Metal/Metal.h>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>

namespace outshine::Render {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t kMatrixWords = 16;
constexpr auto kClearColor = std::array{0.18, 0.25, 0.34};
constexpr size_t kChannels = 4;
using ClipMatrix = std::array<float, kMatrixWords>;

static_assert(sizeof(SurfaceVertex) == 6 * sizeof(float) &&
              alignof(SurfaceVertex) == alignof(float) &&
              offsetof(SurfaceVertex, Normal) == 3 * sizeof(float));
static_assert(sizeof(ClipMatrix) == 64 && sizeof(uint32_t) == 4);

NSString *const kShader = @R"(
#include <metal_stdlib>
using namespace metal;
struct SurfaceVertex {
  packed_float3 position;
  packed_float3 normal;
};
struct Varyings {
  float4 position [[position]];
  float3 normal;
};
vertex Varyings tile_vertex(uint index [[vertex_id]],
                            const device SurfaceVertex *vertices [[buffer(0)]],
                            constant float4x4 &clip [[buffer(1)]]) {
  SurfaceVertex point = vertices[index];
  return {clip * float4(point.position, 1.0), point.normal};
}
fragment float4 tile_fragment(Varyings in [[stage_in]]) {
  float3 normal = normalize(in.normal);
  const float3 light = normalize(float3(-0.55, 0.65, 0.45));
  float diffuse = 0.35 + 0.65 * max(0.0, dot(normal, light));
  float3 reflectance = mix(float3(0.26, 0.22, 0.17), float3(0.13, 0.20, 0.10),
                           smoothstep(0.45, 0.92, normal.y));
  return float4(diffuse * reflectance, 1.0);
}
)";

double ElapsedMs(Clock::time_point since) {
  return std::chrono::duration<double, std::milli>(Clock::now() - since).count();
}

std::expected<ClipMatrix, std::string> MetalClip(Mat4 clip) {
  ClipMatrix packed{};
  for (size_t column = 0; column < kMatrixWords; column += 4) {
    clip[column + 2] = (clip[column + 3] - clip[column + 2]) * 0.5;
  }
  for (size_t word = 0; word < packed.size(); ++word) {
    packed[word] = static_cast<float>(clip[word]);
    if (!std::isfinite(packed[word])) { return std::unexpected("nonfinite Metal clip matrix"); }
  }
  return packed;
}

MTLTextureDescriptor *Texture(MTLPixelFormat format, MTLStorageMode storage) {
  auto *const desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                        width:TileFrameWidth
                                                                       height:TileFrameHeight
                                                                    mipmapped:NO];
  desc.storageMode = storage;
  desc.usage = MTLTextureUsageRenderTarget;
  return desc;
}

struct TilePass {
  id<MTLCommandQueue> Queue;
  id<MTLRenderPipelineState> Pipeline;
  id<MTLDepthStencilState> DepthState;
  MTLRenderPassDescriptor *Pass;
  id<MTLTexture> Color;
};

std::string MetalError(NSError *error, const char *fallback) {
  const char *const reason = error.localizedDescription.UTF8String;
  return reason != nullptr ? reason : fallback;
}

std::expected<TilePass, std::string> MakePass(id<MTLDevice> device) {
  NSError *error = nil;
  const id<MTLLibrary> library = [device newLibraryWithSource:kShader options:nil error:&error];
  if (library == nil) {
    return std::unexpected(MetalError(error, "Metal failed to create a tile shader or pipeline"));
  }
  auto *const descriptor = [[MTLRenderPipelineDescriptor alloc] init];
  descriptor.vertexFunction = [library newFunctionWithName:@"tile_vertex"];
  descriptor.fragmentFunction = [library newFunctionWithName:@"tile_fragment"];
  descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm_sRGB;
  descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  const id<MTLRenderPipelineState> pipeline =
      [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
  if (pipeline == nil) {
    return std::unexpected(MetalError(error, "Metal failed to create a tile shader or pipeline"));
  }
  auto *const depthSpec = [[MTLDepthStencilDescriptor alloc] init];
  depthSpec.depthCompareFunction = MTLCompareFunctionGreater;
  depthSpec.depthWriteEnabled = YES;
  auto *const pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = [device
      newTextureWithDescriptor:Texture(MTLPixelFormatRGBA8Unorm_sRGB, MTLStorageModeShared)];
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].clearColor =
      MTLClearColorMake(kClearColor[0], kClearColor[1], kClearColor[2], 1.0);
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  pass.depthAttachment.texture = [device
      newTextureWithDescriptor:Texture(MTLPixelFormatDepth32Float, MTLStorageModeMemoryless)];
  pass.depthAttachment.loadAction = MTLLoadActionClear;
  pass.depthAttachment.clearDepth = 0.0;
  pass.depthAttachment.storeAction = MTLStoreActionDontCare;
  TilePass result{.Queue = [device newCommandQueue],
                  .Pipeline = pipeline,
                  .DepthState = [device newDepthStencilStateWithDescriptor:depthSpec],
                  .Pass = pass,
                  .Color = pass.colorAttachments[0].texture};
  if (result.Queue == nil || result.DepthState == nil || result.Color == nil ||
      pass.depthAttachment.texture == nil) {
    return std::unexpected("Metal could not allocate the tile pass");
  }
  return result;
}

struct TileMesh {
  id<MTLBuffer> Vertices;
  id<MTLBuffer> Indices;
  size_t IndexCount;
};

std::expected<id<MTLCommandBuffer>, std::string>
SubmitTile(const TilePass &pass, const TileMesh &mesh, const ClipMatrix &clip) {
  @autoreleasepool {
    const id<MTLCommandBuffer> command = [pass.Queue commandBuffer];
    const id<MTLRenderCommandEncoder> draw = [command renderCommandEncoderWithDescriptor:pass.Pass];
    if (draw == nil) { return std::unexpected("Metal could not encode the tile frame"); }
    [draw setRenderPipelineState:pass.Pipeline];
    [draw setDepthStencilState:pass.DepthState];
    [draw setVertexBuffer:mesh.Vertices offset:0 atIndex:0];
    [draw setVertexBytes:clip.data() length:sizeof(clip) atIndex:1];
    [draw drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                     indexCount:mesh.IndexCount
                      indexType:MTLIndexTypeUInt32
                    indexBuffer:mesh.Indices
              indexBufferOffset:0];
    [draw endEncoding];
    [command commit];
    return command;
  }
}

std::expected<std::optional<double>, std::string> RetireTile(id<MTLCommandBuffer> command) {
  [command waitUntilCompleted];
  if (command.status != MTLCommandBufferStatusCompleted) {
    return std::unexpected("Metal tile command failed");
  }
  const double began = command.GPUStartTime;
  const double end = command.GPUEndTime;
  if (!std::isfinite(began) || began <= 0 || !std::isfinite(end) || end <= began) {
    return std::optional<double>{};
  }
  return (end - began) * kMsPerS;
}

std::expected<void, std::string> MeasureTiles(const TilePass &pass,
                                              const TileMesh &mesh,
                                              size_t count,
                                              const TileClipAt &clipAt,
                                              MetalTileFrames &frames) {
  std::array<id<MTLCommandBuffer>, TileFramesInFlight> pending{};
  frames.Samples.resize(count);
  const auto finish = [&](size_t frame) -> std::expected<void, std::string> {
    auto &command = pending[frame % pending.size()];
    const auto elapsed = RetireTile(command);
    if (!elapsed) { return std::unexpected(elapsed.error()); }
    frames.Samples[frame].GpuMs = *elapsed;
    command = nil;
    return {};
  };
  const auto windowAt = Clock::now();
  for (size_t frame = 0; frame < count; ++frame) {
    const auto frameAt = Clock::now();
    const auto matrix = clipAt(frame);
    if (!matrix) { return std::unexpected(matrix.error()); }
    const auto clip = MetalClip(*matrix);
    if (!clip) { return std::unexpected(clip.error()); }
    auto &sample = frames.Samples[frame];
    const auto renderAt = Clock::now();
    if (frame >= pending.size()) {
      const auto retired = finish(frame - pending.size());
      if (!retired) { return retired; }
      sample.SyncWaitMs = ElapsedMs(renderAt);
    }
    const auto submitted = SubmitTile(pass, mesh, *clip);
    if (!submitted) { return std::unexpected(submitted.error()); }
    pending[frame % pending.size()] = *submitted;
    sample.RenderMs = ElapsedMs(renderAt);
    sample.WallMs = ElapsedMs(frameAt);
  }
  frames.WindowMs = ElapsedMs(windowAt);
  const auto flushAt = Clock::now();
  for (size_t frame = count > pending.size() ? count - pending.size() : 0; frame < count; ++frame) {
    if (const auto retired = finish(frame); !retired) { return retired; }
  }
  frames.FlushMs = ElapsedMs(flushAt);
  return {};
}

}

std::expected<MetalTileFrames, std::string>
RenderMetalTile(const SurfaceTile &tile, size_t count, const TileClipAt &clipAt, bool readImage) {
  @autoreleasepool {
    const auto began = Clock::now();
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return std::unexpected("Metal device unavailable"); }
    const auto pass = MakePass(device);
    if (!pass) { return std::unexpected(pass.error()); }
    MetalTileFrames frames;
    const char *const name = device.name.UTF8String;
    frames.Device = name != nullptr ? name : "Metal";
    frames.SetupMs = ElapsedMs(began);
    const auto upload = Clock::now();
    const auto vertexBytes = std::as_bytes(std::span(tile.Vertices));
    const auto indexBytes = std::as_bytes(std::span(tile.Indices));
    const TileMesh mesh{.Vertices = [device newBufferWithBytes:vertexBytes.data()
                                                        length:vertexBytes.size()
                                                       options:MTLResourceStorageModeShared],
                        .Indices = [device newBufferWithBytes:indexBytes.data()
                                                       length:indexBytes.size()
                                                      options:MTLResourceStorageModeShared],
                        .IndexCount = tile.Indices.size()};
    if (mesh.Vertices == nil || mesh.Indices == nil) {
      return std::unexpected("Metal tile upload failed");
    }
    frames.UploadMs = ElapsedMs(upload);
    frames.UploadedBytes = vertexBytes.size() + indexBytes.size();
    if (const auto measured = MeasureTiles(*pass, mesh, count, clipAt, frames); !measured) {
      return std::unexpected(measured.error());
    }
    frames.DeviceId = device.registryID;
    frames.ResourceBytes = device.currentAllocatedSize;
    frames.RecommendedResourceBytes = device.recommendedMaxWorkingSetSize;
    frames.UnifiedMemory = device.hasUnifiedMemory;
    if (readImage) {
      const size_t row = TileFrameWidth * kChannels;
      frames.Rgba.resize(row * TileFrameHeight);
      [pass->Color getBytes:frames.Rgba.data()
                bytesPerRow:row
                 fromRegion:MTLRegionMake2D(0, 0, TileFrameWidth, TileFrameHeight)
                mipmapLevel:0];
    }
    return frames;
  }
}
}
